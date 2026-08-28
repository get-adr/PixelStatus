// Plattformabhaengige Erkennung, ob das Mikrofon gerade benutzt wird.
// Wird vom Auto-Status-Watcher in lib.rs gepollt.

pub use imp::mic_in_use;

// ----- macOS: CoreAudio -----
// Fragt am Standard-Eingabegeraet kAudioDevicePropertyDeviceIsRunningSomewhere ab
// -> true, sobald irgendein Prozess das Mikrofon nutzt (Zoom, Teams, FaceTime, …).
#[cfg(target_os = "macos")]
mod imp {
    use std::ffi::c_void;
    use std::ptr;

    #[repr(C)]
    struct AudioObjectPropertyAddress {
        selector: u32,
        scope: u32,
        element: u32,
    }

    #[link(name = "CoreAudio", kind = "framework")]
    extern "C" {
        fn AudioObjectGetPropertyData(
            in_object: u32,
            in_address: *const AudioObjectPropertyAddress,
            in_qualifier_size: u32,
            in_qualifier: *const c_void,
            io_data_size: *mut u32,
            out_data: *mut c_void,
        ) -> i32;
    }

    const SYSTEM_OBJECT: u32 = 1;
    const DEFAULT_INPUT: u32 = 0x6449_6E20; // 'dIn '  kAudioHardwarePropertyDefaultInputDevice
    const SCOPE_GLOBAL: u32 = 0x676C_6F62; // 'glob'  kAudioObjectPropertyScopeGlobal
    const RUNNING_SOMEWHERE: u32 = 0x676F_6E65; // 'gone' kAudioDevicePropertyDeviceIsRunningSomewhere

    fn get_u32(object: u32, selector: u32) -> Option<u32> {
        let addr = AudioObjectPropertyAddress {
            selector,
            scope: SCOPE_GLOBAL,
            element: 0,
        };
        let mut value: u32 = 0;
        let mut size = std::mem::size_of::<u32>() as u32;
        let status = unsafe {
            AudioObjectGetPropertyData(
                object,
                &addr,
                0,
                ptr::null(),
                &mut size,
                &mut value as *mut u32 as *mut c_void,
            )
        };
        if status == 0 {
            Some(value)
        } else {
            None
        }
    }

    pub fn mic_in_use() -> bool {
        match get_u32(SYSTEM_OBJECT, DEFAULT_INPUT) {
            Some(device) if device != 0 => get_u32(device, RUNNING_SOMEWHERE).unwrap_or(0) != 0,
            _ => false,
        }
    }
}

// ----- Windows: Registry (CapabilityAccessManager) -----
// Pro App steht unter ConsentStore\microphone ein LastUsedTimeStop; Wert 0
// bedeutet "wird gerade genutzt".
#[cfg(target_os = "windows")]
mod imp {
    use winreg::enums::HKEY_CURRENT_USER;
    use winreg::RegKey;

    fn any_active(path: &str) -> bool {
        let hkcu = RegKey::predef(HKEY_CURRENT_USER);
        let store = match hkcu.open_subkey(path) {
            Ok(k) => k,
            Err(_) => return false,
        };
        for name in store.enum_keys().flatten() {
            if let Ok(sub) = store.open_subkey(&name) {
                if let Ok(stop) = sub.get_value::<u64, _>("LastUsedTimeStop") {
                    if stop == 0 {
                        return true;
                    }
                }
            }
        }
        false
    }

    pub fn mic_in_use() -> bool {
        const BASE: &str = r"SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone";
        any_active(BASE) || any_active(&format!(r"{BASE}\NonPackaged"))
    }
}

// ----- Sonstige Plattformen: nicht unterstuetzt -----
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
mod imp {
    pub fn mic_in_use() -> bool {
        false
    }
}
