#include "view/PeopleView.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <random>

static constexpr float kWalkSpeed = 1.35f;        // m/s, typical indoor walking pace
static constexpr float kStepLength = 0.7f;        // m per step
static constexpr float kKeepArrivedFor = 20.0f;   // s of sim time before despawning

PeopleView::Look PeopleView::RandomLook(int id) {
    static const glm::vec3 shirts[] = {{0.85f, 0.25f, 0.25f}, {0.20f, 0.45f, 0.80f}, {0.95f, 0.95f, 0.95f},
                                       {0.25f, 0.60f, 0.45f}, {0.95f, 0.70f, 0.20f}, {0.55f, 0.35f, 0.70f},
                                       {0.30f, 0.30f, 0.35f}, {0.90f, 0.55f, 0.65f}};
    static const glm::vec3 pants[] = {{0.15f, 0.18f, 0.28f}, {0.25f, 0.25f, 0.27f}, {0.45f, 0.38f, 0.30f},
                                      {0.10f, 0.10f, 0.12f}, {0.55f, 0.55f, 0.58f}};
    static const glm::vec3 skins[] = {{0.96f, 0.80f, 0.69f}, {0.87f, 0.67f, 0.52f}, {0.70f, 0.50f, 0.36f},
                                      {0.52f, 0.36f, 0.25f}, {0.36f, 0.24f, 0.17f}};
    static const glm::vec3 hairs[] = {{0.10f, 0.08f, 0.06f}, {0.35f, 0.22f, 0.12f}, {0.75f, 0.60f, 0.35f},
                                      {0.55f, 0.55f, 0.55f}, {0.60f, 0.25f, 0.10f}};
    std::mt19937 rng(static_cast<unsigned>(id) * 2654435761u);
    auto pick = [&](int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); };
    return {shirts[pick(8)], pants[pick(5)], skins[pick(5)], hairs[pick(5)],
            std::uniform_real_distribution<float>(1.58f, 1.88f)(rng)};
}

// Deterministic pseudo-random value in [0, 1) per passenger and purpose.
static float Hash01(int id, int salt) {
    unsigned h = static_cast<unsigned>(id) * 747796405u + static_cast<unsigned>(salt) * 2891336453u;
    h ^= h >> 16; h *= 2246822519u; h ^= h >> 13;
    return (h & 0xFFFFFF) / float(0x1000000);
}

glm::vec3 PeopleView::SpawnPoint(const sim::Passenger& p) const {
    float y = m_layout.FloorY(p.origin);
    if (p.origin == 0)   // arrive from the street
        return {(Hash01(p.id, 1) - 0.5f) * 16.0f, y, m_layout.frontZ + 3.5f};
    float side = Hash01(p.id, 2) < 0.5f ? -1.0f : 1.0f;   // come from a desk area
    return {side * (m_layout.halfWidth - 2.0f), y, -1.5f + Hash01(p.id, 3) * 7.0f};
}

glm::vec3 PeopleView::ExitPoint(const sim::Passenger& p) const {
    float y = m_layout.FloorY(p.destination);
    if (p.destination == 0)
        return {(Hash01(p.id, 4) - 0.5f) * 16.0f, y, m_layout.frontZ + 4.0f};
    float side = Hash01(p.id, 5) < 0.5f ? -1.0f : 1.0f;
    return {side * (m_layout.halfWidth - 2.0f), y, -1.5f + Hash01(p.id, 6) * 7.0f};
}

// Which car a waiting passenger should queue in front of.
int PeopleView::CarServing(const sim::ElevatorSystem& system, int floor, sim::Direction d) const {
    const sim::HallCall& call = system.GetHallCall(floor, d);
    if (call.active && call.assignedCar >= 0) return call.assignedCar;
    for (const sim::Car& car : system.Cars()) {
        bool here = std::abs(car.Position() - m_layout.FloorY(floor)) < 0.2f;
        bool open = !car.Doors().IsClosed();
        if (here && open && (car.direction == d || car.direction == sim::Direction::None)) return car.Id();
    }
    return -1;
}

void PeopleView::Update(const sim::ElevatorSystem& system, float simDt) {
    const auto& cars = system.Cars();
    std::unordered_map<int, int> queueLength;   // key: floor * 64 + car, value: people queued so far

    for (const sim::Passenger& p : system.Passengers()) {
        if (m_departed.count(p.id)) continue;
        bool expired = p.state == sim::PassengerState::Arrived && system.Time() - p.arriveTime > kKeepArrivedFor;
        if (expired) { m_people.erase(p.id); m_departed.insert(p.id); continue; }

        auto [it, isNew] = m_people.try_emplace(p.id);
        Person& person = it->second;
        if (isNew) {
            person.look = RandomLook(p.id);
            person.position = SpawnPoint(p);
        }

        // --- Where should this person be? ---
        float restingYaw = glm::pi<float>();   // facing the elevator doors (-z)
        bool insideMovingCar = false;
        switch (p.state) {
        case sim::PassengerState::Waiting: {
            int car = CarServing(system, p.origin, p.Dir());
            int slot = queueLength[p.origin * 64 + car + 1]++;
            float baseX = car >= 0 ? m_layout.ShaftX(car) : 0.0f;
            person.target = {baseX + ((slot % 4) - 1.5f) * 0.55f,
                             m_layout.FloorY(p.origin),
                             1.1f + (slot / 4) * 0.6f};
            break;
        }
        case sim::PassengerState::Riding: {
            const sim::Car& car = cars[p.car];
            auto seat = std::find(car.riders.begin(), car.riders.end(), p.id) - car.riders.begin();
            int slot = static_cast<int>(seat);
            person.target = {m_layout.ShaftX(p.car) + ((slot % 5) - 2.0f) * 0.34f,
                             car.Position(),
                             m_layout.carCenterZ - 0.55f + (slot / 5) * 0.28f};
            restingYaw = 0.0f;                 // riders face the car door
            insideMovingCar = car.IsRunning();
            break;
        }
        case sim::PassengerState::Arrived:
            person.target = ExitPoint(p);
            person.leaving = true;
            break;
        }

        // --- Move toward it ---
        if (insideMovingCar) {
            person.position = person.target;   // carried by the car
            person.stride = 0.0f;
            continue;
        }
        glm::vec3 toTarget = person.target - person.position;
        toTarget.y = 0.0f;
        float distance = glm::length(toTarget);
        person.position.y = person.target.y;

        // Normal walking pace, faster when catching up at high simulation speed.
        float speed = std::min(std::max(kWalkSpeed, distance * 0.8f), 6.0f);
        float step = std::min(distance, speed * simDt);
        bool walking = step > 1e-4f;
        if (walking) {
            person.position += toTarget / distance * step;
            person.walkPhase += step / kStepLength * glm::pi<float>();
        }
        float strideGoal = walking ? 1.0f : 0.0f;
        person.stride += (strideGoal - person.stride) * std::min(1.0f, simDt * 8.0f);

        // Turn smoothly toward the walking direction, or the resting direction.
        float desiredYaw = walking ? std::atan2(toTarget.x, toTarget.z) : restingYaw;
        float diff = std::remainder(desiredYaw - person.yaw, 2.0f * glm::pi<float>());
        person.yaw += diff * std::min(1.0f, simDt * 8.0f);

        if (person.leaving && distance < 0.2f) {
            m_people.erase(it);
            m_departed.insert(p.id);
        }
    }
}

void PeopleView::AppendDrawItems(const MeshLibrary& meshes, std::vector<DrawItem>& items) const {
    const glm::vec3 xAxis(1, 0, 0), yAxis(0, 1, 0);

    for (const auto& [id, person] : m_people) {
        const Look& look = person.look;
        const float h = look.height;
        const float legLength = 0.46f * h, torsoLength = 0.31f * h, armLength = 0.35f * h;
        const float headSize = 0.13f * h;
        const float build = h / 1.75f;

        // Walk cycle: legs swing opposite each other, arms counter-swing, and
        // the body rises slightly over the planted leg.
        float swing = std::sin(person.walkPhase) * 0.55f * person.stride;
        float bob = std::abs(std::cos(person.walkPhase)) * 0.035f * person.stride;

        // Forward kinematics: each joint's transform is its parent's transform
        // times a local offset and rotation.
        glm::mat4 root = glm::translate(glm::mat4(1.0f), person.position + glm::vec3(0.0f, bob, 0.0f));
        root = glm::rotate(root, person.yaw, yAxis);
        glm::mat4 hips = glm::translate(root, {0.0f, legLength, 0.0f});
        glm::mat4 shoulders = glm::translate(hips, {0.0f, torsoLength - 0.04f, 0.0f});

        auto limb = [&](const glm::mat4& joint, float side, float angle, float length, float thickness,
                        const glm::vec3& color) {
            glm::mat4 m = glm::translate(joint, {side, 0.0f, 0.0f});
            m = glm::rotate(m, angle, xAxis);
            m = glm::translate(m, {0.0f, -length * 0.5f, 0.0f});
            items.push_back({&meshes.cube, glm::scale(m, {thickness, length, thickness * 1.1f}), color, Material::Matte, true});
        };
        limb(hips, -0.085f * build, swing, legLength, 0.12f * build, look.pants);
        limb(hips, 0.085f * build, -swing, legLength, 0.12f * build, look.pants);
        limb(shoulders, -0.21f * build, -swing * 0.8f, armLength, 0.085f * build, look.shirt);
        limb(shoulders, 0.21f * build, swing * 0.8f, armLength, 0.085f * build, look.shirt);

        glm::mat4 torso = glm::translate(hips, {0.0f, torsoLength * 0.5f, 0.0f});
        items.push_back({&meshes.cube, glm::scale(torso, {0.34f * build, torsoLength, 0.2f * build}),
                         look.shirt, Material::Matte, true});

        glm::mat4 head = glm::translate(shoulders, {0.0f, 0.05f + headSize * 0.55f, 0.0f});
        items.push_back({&meshes.sphere, glm::scale(head, glm::vec3(headSize)), look.skin, Material::Matte, true});
        glm::mat4 hair = glm::translate(head, {0.0f, headSize * 0.14f, -headSize * 0.05f});
        items.push_back({&meshes.sphere, glm::scale(hair, {headSize * 1.06f, headSize * 0.8f, headSize * 1.06f}),
                         look.hair, Material::Matte, true});
    }
}
