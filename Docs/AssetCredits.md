# Asset credits

Generated from Data/AssetManifest.json by Scripts/FetchAssets.py. Every asset below allows redistribution in a game.
Assets under the Fab Standard License or Epic template content are not stored in this public repository (see the end).

| Asset | Use in Riskbound | Source | Author(s) | Licence |
|---|---|---|---|---|
| [Herringbone Parquet](https://polyhaven.com/a/herringbone_parquet) | Parquet floor: reception, manager's office, boardroom | Poly Haven | Jenelle van Heerden, Sergej Majboroda | CC0 1.0 |
| [Laminate Floor 02](https://polyhaven.com/a/laminate_floor_02) | Work-area floor: analyst workspace, training, monitoring | Poly Haven | Charlotte Baglioni, Dario Barresi | CC0 1.0 |
| [Smooth Concrete Floor](https://polyhaven.com/a/smooth_concrete_floor) | Corridor floor | Poly Haven | Dimitrios Savva | CC0 1.0 |
| [Plastered Wall 04](https://polyhaven.com/a/plastered_wall_04) | Interior walls | Poly Haven | Rob Tuytel | CC0 1.0 |
| [Oak Veneer 01](https://polyhaven.com/a/oak_veneer_01) | Light oak desks and tables | Poly Haven | Jenelle van Heerden | CC0 1.0 |
| [Black Walnut Veneer 01](https://polyhaven.com/a/black_walnut_veneer_01) | Walnut: manager's desk, boardroom table | Poly Haven | Jenelle van Heerden | CC0 1.0 |
| [Poly Wool Herringbone](https://polyhaven.com/a/poly_wool_herringbone) | Acoustic wall panels, soft furnishings | Poly Haven | Rico Cilliers, colormass | CC0 1.0 |
| [Terrazzo Tiles](https://polyhaven.com/a/terrazzo_tiles) | Reception feature floor | Poly Haven | Amal Kumar | CC0 1.0 |
| [Modern Arm Chair 01](https://polyhaven.com/a/modern_arm_chair_01) | Task and visitor chairs | Poly Haven | Vibrant Nordic | CC0 1.0 |
| [Dining Chair 02](https://polyhaven.com/a/dining_chair_02) | Boardroom and meeting chairs | Poly Haven | James Ray Cock | CC0 1.0 |
| [Sofa 02](https://polyhaven.com/a/sofa_02) | Reception sofa | Poly Haven | Kirill Sannikov | CC0 1.0 |
| [Modern Coffee Table 01](https://polyhaven.com/a/modern_coffee_table_01) | Reception coffee table | Poly Haven | Amin | CC0 1.0 |
| [Potted Plant 02](https://polyhaven.com/a/potted_plant_02) | Large plants | Poly Haven | Rico Cilliers | CC0 1.0 |
| [Potted Plant 04](https://polyhaven.com/a/potted_plant_04) | Desk plants | Poly Haven | James Ray Cock | CC0 1.0 |
| [Drawer Cabinet](https://polyhaven.com/a/drawer_cabinet) | Filing cabinet (insurance schedule evidence) | Poly Haven | Ulan Cabanilla | CC0 1.0 |
| [Binder Notebook](https://polyhaven.com/a/binder_notebook) | Maintenance binder (evidence) | Poly Haven | DaDrood | CC0 1.0 |
| [Clipboard](https://polyhaven.com/a/clipboard) | Manual temperature log clipboard (evidence) | Poly Haven | ProgrammerOnCoffee | CC0 1.0 |
| [Office Notepads](https://polyhaven.com/a/office_notepads) | Document wallet / desk notepads (evidence, dressing) | Poly Haven | Ulan Cabanilla | CC0 1.0 |
| [Wall Clock](https://polyhaven.com/a/wall_clock) | Wall clocks | Poly Haven | PierreB3D | CC0 1.0 |
| [Projector Screen](https://polyhaven.com/a/projector_screen) | Meeting room screen | Poly Haven | PeterM | CC0 1.0 |
| [Modern Ceiling Lamp 01](https://polyhaven.com/a/modern_ceiling_lamp_01) | Boardroom pendant lights | Poly Haven | James Ray Cock | CC0 1.0 |
| [Canary Wharf](https://polyhaven.com/a/canary_wharf) | Business-district view through the windows, sky light | Poly Haven | Andreas Mischok | CC0 1.0 |
| [Inter 4.1](https://rsms.me/inter/) | UI typeface | rsms/inter (Rasmus Andersson) | Rasmus Andersson | SIL Open Font License 1.1 |

## Original Riskbound textures
Screen UIs (dashboard, spreadsheet, inbox, alarm console), the training whiteboard sketch and the three abstract wall prints are drawn by `Scripts/GenerateTextures.py` for this project (all names invented; no Mission 001 figures). Office task chairs, desks, monitors, shelving, lamps and small props are built from engine shapes in `ARbValtrenHQ`.

## Logo and brand check
Every model and texture above was inspected for real-world logos or brand text before use (contact sheet of all diffuse maps, then zoomed crops).
- Wall Clock: dial printed "URBAN quartz" - painted out (Scripts/FetchAssets.py EDITS; the importer uses Data/Assets/edited/).
- Projector Screen: housing printed "SIRIUS steering" - painted out the same way.
- Stationery Supplies: several labels printed "Haven Pinworks" - model excluded rather than edited.
- All other assets: no text or logos found.

## Not in the repository
- UE mannequin (Priya Okafor fallback), Epic template content: copied from the local engine install by `Scripts/CopyEngineContent.bat` into `Content/Characters/` (git-ignored).
