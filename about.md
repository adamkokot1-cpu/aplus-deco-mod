# A+ Deco Mod

**An AI that decorates your levels.** Type what you want, pick how many objects, press **Generate**.

Open any level in the editor and press the **A+** button.

## What it does
- Reads your description: themes (*neon, forest, lava, ice, space, spooky, ocean, cute, party, desert, minimal, dungeon, factory*), colors (*"pink and cyan"*, *"dark red"*), objects (*stars, hearts, clouds, chains, grass...*), and words like *no clouds*, *huge*, *dense*, *chaotic*
- Looks at your blocks and decorates **around** them: grass and bushes on top of blocks, chains and icicles under ceilings, particles near the gameplay, clouds in the sky and a non-solid skyline behind everything
- Places **exactly the number of objects you choose**
- Adds theme color triggers at the start of the level (5 channels, default 50-54)
- Never adds hitboxes: everything is decoration or no-touch

## Buttons
- **Idea**: random example description
- **Area**: whole level, current view, or around your selection
- **AI Plan**: see what the AI understood
- **Undo Last**: removes the last generation (Ctrl+Z works too)

## Claude AI (optional)
Turn on **Claude AI (online)** and paste an Anthropic API key in the mod settings to let Claude design the theme. Without a key, the built-in offline AI does it.
