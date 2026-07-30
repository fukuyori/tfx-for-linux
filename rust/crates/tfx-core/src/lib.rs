use casefold::simple_fold;

pub const MAX_NATIVE_PATH_BYTES: usize = 1024 * 1024;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NativePathEncoding {
    UnixBytes,
    WindowsUtf16Le,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NativePath {
    pub encoding: NativePathEncoding,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NativePathError {
    InvalidInput,
    UnsupportedEncoding,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypeAheadStep {
    pub row: i32,
    pub prefix: String,
}

pub fn round_trip_native_path(path: NativePath) -> Result<NativePath, NativePathError> {
    validate_native_path(&path)?;

    #[cfg(unix)]
    {
        use std::ffi::OsString;
        use std::os::unix::ffi::OsStringExt;
        use std::path::PathBuf;

        if path.encoding != NativePathEncoding::UnixBytes {
            return Err(NativePathError::UnsupportedEncoding);
        }

        let native = PathBuf::from(OsString::from_vec(path.data));
        return Ok(NativePath {
            encoding: NativePathEncoding::UnixBytes,
            data: native.into_os_string().into_vec(),
        });
    }

    #[cfg(windows)]
    {
        use std::ffi::OsString;
        use std::os::windows::ffi::{OsStrExt, OsStringExt};
        use std::path::PathBuf;

        if path.encoding != NativePathEncoding::WindowsUtf16Le {
            return Err(NativePathError::UnsupportedEncoding);
        }

        let wide: Vec<u16> = path
            .data
            .chunks_exact(2)
            .map(|bytes| u16::from_le_bytes([bytes[0], bytes[1]]))
            .collect();
        let native = PathBuf::from(OsString::from_wide(&wide));
        let data = native
            .as_os_str()
            .encode_wide()
            .flat_map(u16::to_le_bytes)
            .collect();
        return Ok(NativePath {
            encoding: NativePathEncoding::WindowsUtf16Le,
            data,
        });
    }

    #[allow(unreachable_code)]
    Err(NativePathError::UnsupportedEncoding)
}

fn validate_native_path(path: &NativePath) -> Result<(), NativePathError> {
    if path.data.len() > MAX_NATIVE_PATH_BYTES {
        return Err(NativePathError::InvalidInput);
    }

    match path.encoding {
        NativePathEncoding::UnixBytes => {
            if path.data.contains(&0) {
                return Err(NativePathError::InvalidInput);
            }
        }
        NativePathEncoding::WindowsUtf16Le => {
            if path.data.len() % 2 != 0 {
                return Err(NativePathError::InvalidInput);
            }
            if path
                .data
                .chunks_exact(2)
                .any(|bytes| u16::from_le_bytes([bytes[0], bytes[1]]) == 0)
            {
                return Err(NativePathError::InvalidInput);
            }
        }
    }

    Ok(())
}

pub fn type_ahead_step(
    names: &[String],
    current_row: i32,
    prefix: &str,
    typed: &str,
) -> TypeAheadStep {
    let mut step = TypeAheadStep {
        row: -1,
        prefix: prefix.to_owned(),
    };
    if names.is_empty() || typed.is_empty() {
        return step;
    }

    if prefix.encode_utf16().count() == 1 && caseless_eq(prefix, typed) {
        let rows = names.len() as i64;
        for offset in 1..=rows {
            let row = (i64::from(current_row) + offset + rows).rem_euclid(rows) as usize;
            if caseless_starts_with(&names[row], prefix) {
                step.row = row as i32;
                break;
            }
        }
        return step;
    }

    let candidate = prefix.to_owned() + typed;
    if let Some(row) = names
        .iter()
        .position(|name| caseless_starts_with(name, &candidate))
    {
        step.row = row as i32;
        step.prefix = candidate;
    }
    step
}

fn caseless_eq(left: &str, right: &str) -> bool {
    fold(left) == fold(right)
}

fn caseless_starts_with(value: &str, prefix: &str) -> bool {
    fold(value).starts_with(&fold(prefix))
}

fn fold(value: &str) -> String {
    simple_fold(value.to_owned())
}

#[cfg(test)]
mod tests {
    use super::{
        round_trip_native_path, type_ahead_step, NativePath, NativePathEncoding, NativePathError,
        TypeAheadStep, MAX_NATIVE_PATH_BYTES,
    };

    fn names(values: &[&str]) -> Vec<String> {
        values.iter().map(|value| (*value).to_owned()).collect()
    }

    #[test]
    fn first_keystroke_jumps_to_first_match() {
        assert_eq!(
            type_ahead_step(&names(&["alpha", "beta", "charlie", "chip"]), -1, "", "c"),
            TypeAheadStep {
                row: 2,
                prefix: "c".to_owned(),
            }
        );
    }

    #[test]
    fn extended_prefix_narrows_the_match() {
        let values = names(&["alpha", "beta", "charlie", "chip"]);
        assert_eq!(type_ahead_step(&values, 2, "c", "h").row, 2);
        assert_eq!(
            type_ahead_step(&values, 2, "ch", "i"),
            TypeAheadStep {
                row: 3,
                prefix: "chi".to_owned(),
            }
        );
    }

    #[test]
    fn mismatch_keeps_prefix_and_selection() {
        assert_eq!(
            type_ahead_step(&names(&["alpha", "beta", "charlie"]), 2, "ch", "z"),
            TypeAheadStep {
                row: -1,
                prefix: "ch".to_owned(),
            }
        );
    }

    #[test]
    fn repeated_initial_cycles_and_wraps() {
        let values = names(&["docs", "cats", "code", "songs", "cli"]);
        assert_eq!(type_ahead_step(&values, 1, "c", "c").row, 2);
        assert_eq!(type_ahead_step(&values, 2, "c", "c").row, 4);
        assert_eq!(type_ahead_step(&values, 4, "c", "c").row, 1);
    }

    #[test]
    fn matching_uses_unicode_simple_case_folding() {
        let values = names(&["README.md", "Straße.txt", "STRASSE.md", "日本語.txt"]);
        assert_eq!(type_ahead_step(&values, -1, "", "r").row, 0);
        assert_eq!(type_ahead_step(&values, -1, "", "STRASS").row, 2);
        assert_eq!(type_ahead_step(&values, -1, "", "日").row, 3);
    }

    #[test]
    fn empty_input_does_nothing() {
        assert_eq!(
            type_ahead_step(&[], -1, "", "a"),
            TypeAheadStep {
                row: -1,
                prefix: String::new(),
            }
        );
        assert_eq!(
            type_ahead_step(&names(&["alpha"]), 0, "a", ""),
            TypeAheadStep {
                row: -1,
                prefix: "a".to_owned(),
            }
        );
    }

    #[test]
    fn out_of_range_current_row_still_cycles_safely() {
        let values = names(&["alpha", "atom"]);
        assert_eq!(type_ahead_step(&values, i32::MAX, "a", "A").row, 0);
        assert_eq!(type_ahead_step(&values, i32::MIN, "a", "A").row, 1);
    }

    #[cfg(unix)]
    #[test]
    fn unix_path_round_trip_preserves_non_utf8_bytes() {
        let data = b"/tmp/report-\xff.txt".to_vec();
        assert_eq!(
            round_trip_native_path(NativePath {
                encoding: NativePathEncoding::UnixBytes,
                data: data.clone(),
            }),
            Ok(NativePath {
                encoding: NativePathEncoding::UnixBytes,
                data,
            })
        );
    }

    #[test]
    fn native_path_rejects_embedded_nul() {
        assert_eq!(
            round_trip_native_path(NativePath {
                encoding: NativePathEncoding::UnixBytes,
                data: b"/tmp/a\0b".to_vec(),
            }),
            Err(NativePathError::InvalidInput)
        );
    }

    #[test]
    fn windows_path_rejects_odd_byte_count() {
        assert_eq!(
            round_trip_native_path(NativePath {
                encoding: NativePathEncoding::WindowsUtf16Le,
                data: vec![b'C', 0, b':'],
            }),
            Err(NativePathError::InvalidInput)
        );
    }

    #[test]
    fn native_path_rejects_oversized_input() {
        assert_eq!(
            round_trip_native_path(NativePath {
                encoding: NativePathEncoding::UnixBytes,
                data: vec![b'a'; MAX_NATIVE_PATH_BYTES + 1],
            }),
            Err(NativePathError::InvalidInput)
        );
    }

    #[cfg(unix)]
    #[test]
    fn unix_target_rejects_windows_encoding() {
        assert_eq!(
            round_trip_native_path(NativePath {
                encoding: NativePathEncoding::WindowsUtf16Le,
                data: vec![b'C', 0, b':', 0],
            }),
            Err(NativePathError::UnsupportedEncoding)
        );
    }
}
