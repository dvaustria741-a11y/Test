# UI asset prompts (ChatGPT image)

The game already draws a full procedural UI with its own built-in text, so everything works
without any art. Drop PNG files with these exact names into `app/src/main/assets/ui/` and the
game uses them automatically. All sprites are PNG with a transparent background.
IMPORTANT: no text, letters or numbers inside any image. The game draws the words
(PLAY, SETTINGS, ...) on top of the buttons itself.

Shared style line, paste it at the end of every prompt:
"Stylized low-poly nature game UI, soft rounded shapes, warm cream and leaf-green palette,
thin gold outline, subtle top highlight and soft bottom shadow, hand-painted flat vector look,
cozy adventure game, clean edges, transparent background, no text, no letters, no numbers."

1. title.png  (1024x512, game logo shown on the intro and main menu)
   "Game logo emblem for a cozy exploration game, a big chunky wooden sign shape with
   carved leaf decorations, a small rolling green hill with an orange autumn tree and a
   green pine tree growing out of the top of the sign, a banner area left completely blank,
   [shared style line]"
   Note: if you want the words NATURE GAME inside the logo, add them to the image and the
   game will show your logo instead of its own text.

2. btn_wide.png  (640x200, used for PLAY / SETTINGS / QUIT / BACK / RESUME buttons)
   "A wide rounded rectangle game button, glossy leaf-green with a lighter top half,
   thin gold border, tiny leaf ornaments at the left and right ends, completely empty in the
   middle so text can be placed on it, [shared style line]"

3. panel.png  (1024x768, background of the Settings and Pause windows)
   "A large rounded rectangle menu panel, dark forest-green wood with a subtle grain,
   thin gold border, small vine and leaf corner decorations, empty center, slightly
   transparent look, [shared style line]"

4. joy_base.png  (256x256)
   "Circular virtual joystick base, frosted wood ring with a thin gold outline and an engraved
   leaf pattern in the groove, hollow center, [shared style line]"

5. joy_knob.png  (256x256)
   "Round joystick thumb knob, glossy leaf-green gem-like sphere with a soft highlight and a
   thin gold rim, [shared style line]"

6. btn_jump.png  (256x256)
   "Round jump button, cream circle with a thin gold rim and a bold green upward arrow icon
   in the middle, [shared style line]"

7. btn_gear.png  (256x256)
   "Round pause button, dark green circle with a thin gold rim and a clean cream gear icon in
   the middle, [shared style line]"

8. App icon (512x512, set it as the launcher icon later, not loaded by the game)
   "Mobile game app icon, stylized low-poly landscape with an orange autumn tree, a green pine
   tree and a sandy rock formation, bright blue sky with soft clouds, rounded square,
   [shared style line without the transparent background part]"
