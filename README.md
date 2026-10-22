# Auto Decorate (Geometry Dash 2.2081, Geode v5)

Select blocks in the editor, open the menu, pick a style, press **Confirm**, then **Decorate**.
Decoration is added along the open edges of the selected blocks, in the style you chose.

## Using it

- Open the menu with the **Deco** button on the editor screen, or the hotkey (default
  `Ctrl+Shift+D`, change it in the mod's settings).
- **Style Source**
  - *This Level*: learns from the decoration already in your level.
  - *Online Level*: type a level ID or name.
  - *Player*: type the ID of any level made by that player. The mod also studies up to 3 more
    of that player's levels.
- **Confirm** loads the style. **Decorate** unlocks once it is ready and only touches the
  blocks in your current selection.
- Sliders and toggles (every one has an info button): Density, Flow, Top / Bottom / Sides, Clean.
- Everything it places goes on the undo list.

## Getting the .geode file (no local setup needed)

1. Create a GitHub repo and push this folder to it.
2. Open the repo's **Actions** tab. The *Build Geode Mod* workflow runs on every push.
3. When it finishes, download the **Build Output** artifact. It contains one `.geode` file with
   Windows, macOS and Android builds combined.
4. Put the `.geode` file in your Geometry Dash `geode/mods` folder (or drag it into Geode's mod list).

Before building, edit `mod.json` and change `yourname` in `id` and `developer`.

### Building locally instead

Install the Geode CLI and SDK, set `GEODE_SDK`, then run `geode build`.

## How it works

1. **Learn**: for each decoration object in the source, find the nearest block and which side of
   it the object sits on. Store its offset, rotation, scale, layer and colours relative to that
   block face.
2. **Detect**: look at your selection. Solid blocks get decorated. Hazards, slopes, portals,
   triggers and existing decoration are skipped.
3. **Place**: find exposed block edges, group them into continuous runs, pick a "theme" object
   per run, and place learned samples along it with a soft rotation wave (Flow) and snapping
   (Clean).

## Known limits

- Never compiled by the author. The first CI run may report API mismatches that need small fixes.
- Slopes count as solid cells but are not decorated yet.
- Player mode needs a level ID by that player, not a username.
