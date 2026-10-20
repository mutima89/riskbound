# Art direction

Riskbound should feel like a well-run, slightly too quiet corporate headquarters on an ordinary weekday: credible, calm, expensive in its restraint. Tension comes from the work, not from the room.

## References (what we take, what we leave)
| Reference | Take | Leave |
|---|---|---|
| Control (Oldest House, Federal Bureau offices) | Brutalist-modern volumes, strong bounce light off pale concrete, hard key light with soft fill, signage as architecture | Supernatural distortion, red emergency palette |
| Severance (Lumon offices) | Clean geometry, very controlled palette, green-grey carpet vs white walls, unsettling tidiness, low partitions | Sterile emptiness: our offices are lived in |
| Mirror's Edge (corporate interiors) | Bright neutral base with one saturated accent, glass, reflections, clean silhouettes | Pure white blowout |
| Hitman (Paris/Dubai/Berlin office floors) | Believable office density: plants, binders, mugs, cable trays; mixed warm practical lights | Luxury excess |
| The Stanley Parable Ultra Deluxe | Readable rooms with clear labels and doors, office clutter that tells you what people do there | Satire |
| Premium diegetic/workstation UI (Alien Isolation terminals, Control's terminals, modern analytics dashboards) | Dark glass panels, crisp sans-serif, tabular numbers, one accent colour, information density with hierarchy | CRT nostalgia |

## Palette (scene)
- Base: warm white plaster `#E9E6E0`, pale concrete `#B8B5AE`, graphite `#2B2E33`.
- Wood: light oak (desks, meeting table), dark walnut (manager's office, boardroom).
- Floors: oak parquet in reception/boardroom/manager's office, smooth concrete in the corridor, laminate in work areas, terrazzo optional.
- Accent: Valtren teal `#2BA3A8` (signage strips, chair fabric, UI accent). Warning amber `#E3A33B` and alert red `#D9534F` only for risk states.

## Materials
PBR, CC0 sources only (Poly Haven, ambientCG). Roughness kept 0.35-0.8 except glass, chrome and screens. World-aligned UVs on architecture so tiling never stretches. Glass partitions: thin, low-roughness, 10-20% opacity with a frosted band at eye height in meeting rooms.

## Lighting mood
Daylight first: a late-morning sun through full-height windows on the outer walls (HDRI city backdrop), Lumen GI + reflections, skylight from the HDRI. Interior fill from recessed ceiling panels (cool 4500 K) and practical lamps (warm 3000 K) in the manager's office and boardroom. Fixed exposure range so rooms do not pump when looking at windows. Monitoring room is the exception: dimmer, lit by screens.

## Typography
Inter (SIL OFL) for all UI; Inter SemiBold for headings, Inter Regular for body, tabular figures for numbers. Fallback: engine Roboto. No more than 3 sizes per screen.

## UI rules
- Dark glass panels (`#0E1116` at 92%), 1 px hairline borders (`#FFFFFF` at 8%), 6 px corner feel via padding, generous spacing on a 4 px grid.
- One accent colour (teal) for focus and primary actions; amber/red only for KRI/risk status.
- Every screen: title, one-line purpose, content, actions bottom-right. Keyboard focus is always visible (teal outline).
- Motion: 120-180 ms fades/slides; disabled when Reduced Motion is on.
- Numbers: money with thousands separators; EL always shown with worst case; ordinal scores labelled as ordinal.

## Fictional names
All companies, suppliers, banks, insurers, regulators-by-brand, products, logos and visible text in the game are invented. Before adding a name, web-search it and pick another if a real business uses it (or something confusingly close). Downloaded assets are inspected for real-world logos or brand text (monitors, laptops, packaging, signage, vehicles); any that show one are retextured or excluded, and the decision is noted in Docs/AssetCredits.md. Renamed in save v4: Valtren Coldline Logistics, Orrinbrook Mutual Assurance, Quellard Grid Projects, Kelvarin Refrigeration Services.

## Assets wanted from Fab / Quixel / MetaHuman (need the user's Epic sign-in)
Modern office desk and task-chair set, monitors/keyboards, acoustic ceiling and wall panels, carpet tiles, a business-dressed MetaHuman for Priya Okafor. Free Fab Standard License assets only; kept out of the public repo (see Docs/AssetCredits.md).
