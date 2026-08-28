// Verhindert ein zusaetzliches Konsolenfenster unter Windows im Release-Build.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    pixelstatus_companion_lib::run()
}
