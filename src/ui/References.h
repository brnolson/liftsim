#pragma once

#include <cstring>

// Engineering sources the simulation is based on. Listed in the reference
// panel (key I) and cited by the component inspector; clicking an entry opens
// its page in the browser.
struct Reference {
    const char* id;
    const char* citation;
    const char* url;
};

inline constexpr Reference kReferences[] = {
    {"EN81-20", "EN 81-20:2020  Safety rules for lifts, Part 20: Passenger and goods passenger lifts",
     "https://www.evs.ee/en/evs-en-81-20-2020"},
    {"A17.1", "ASME A17.1-2022 / CSA B44-22  Safety Code for Elevators and Escalators",
     "https://webstore.ansi.org/standards/csa/csaasmea172022b44"},
    {"CIBSE-D", "CIBSE Guide D:2020  Transportation Systems in Buildings, 6th ed.",
     "https://cibse.org/knowledge-research/knowledge-portal/guide-d-transportation-systems-in-buildings-2020"},
    {"KONE-EN81", "KONE, Elevator standard EN 81-20 fact sheet (UCM, ascending car overspeed)",
     "https://www.kone.com.au/Images/pdf_Safety%20Standard%20EN81-20%20Fact%20Sheet_tcm46-29613.pdf"},
    {"DEMAND", "Lift Passenger Demand in Office Buildings, Elevator World (traffic mix surveys)",
     "https://elevatorworld.com/?p=41334"},
    {"PETERS", "R. D. Peters, Ideal Lift Kinematics, Elevator Technology 6 (1995)",
     "https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000329.html"},
    {"BARNEY", "G. Barney & L. Al-Sharif, Elevator Traffic Handbook, 2nd ed., Routledge (2016)",
     "https://www.routledge.com/Elevator-Traffic-Handbook-Theory-and-Practice/Barney-Al-Sharif/p/book/9781032179650"},
    {"WIEK", "L. Wiek, On Friction for Traction of Elevators (1996)",
     "https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000362.html"},
    {"HYMANS", "Fred Hymans and the Theory of Rope Traction, Elevator World",
     "https://elevatorworld.com/?p=26460"},
    {"COMP", "US 8,360,212 B2  Compensating ropes of an elevator (imbalance above 30-40 m travel)",
     "https://patents.google.com/patent/US8360212"},
    {"OLEO", "Oleo International, Elevator buffers: stroke and energy absorption",
     "https://www.oleoelevator.com/elevator-safety/"},
};

inline const Reference* FindReference(const char* id) {
    for (const Reference& r : kReferences)
        if (std::strcmp(r.id, id) == 0) return &r;
    return nullptr;
}
