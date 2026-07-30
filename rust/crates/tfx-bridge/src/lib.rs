use std::panic::{catch_unwind, AssertUnwindSafe};

#[cxx::bridge(namespace = "tfx::rust_bridge")]
mod ffi {
    #[derive(Debug)]
    enum BridgeErrorCode {
        Ok = 0,
        InvalidInput = 1,
        Unsupported = 2,
        Internal = 3,
    }

    #[derive(Debug)]
    enum NativePathEncoding {
        UnixBytes = 0,
        WindowsUtf16Le = 1,
    }

    struct NativePath {
        encoding: NativePathEncoding,
        data: Vec<u8>,
    }

    struct NativePathResult {
        path: NativePath,
        error: BridgeErrorCode,
    }

    struct TypeAheadResult {
        row: i32,
        prefix: String,
        error: BridgeErrorCode,
    }

    extern "Rust" {
        fn round_trip_native_path(path: NativePath) -> NativePathResult;

        fn type_ahead_step(
            names: Vec<String>,
            current_row: i32,
            prefix: String,
            typed: String,
        ) -> TypeAheadResult;
    }
}

fn round_trip_native_path(path: ffi::NativePath) -> ffi::NativePathResult {
    let encoding = path.encoding;
    let core_encoding = match encoding {
        ffi::NativePathEncoding::UnixBytes => tfx_core::NativePathEncoding::UnixBytes,
        ffi::NativePathEncoding::WindowsUtf16Le => tfx_core::NativePathEncoding::WindowsUtf16Le,
        _ => return native_path_error(encoding, ffi::BridgeErrorCode::InvalidInput),
    };

    match catch_boundary(|| {
        tfx_core::round_trip_native_path(tfx_core::NativePath {
            encoding: core_encoding,
            data: path.data,
        })
    }) {
        Ok(Ok(result)) => ffi::NativePathResult {
            path: ffi::NativePath {
                encoding,
                data: result.data,
            },
            error: ffi::BridgeErrorCode::Ok,
        },
        Ok(Err(tfx_core::NativePathError::InvalidInput)) => {
            native_path_error(encoding, ffi::BridgeErrorCode::InvalidInput)
        }
        Ok(Err(tfx_core::NativePathError::UnsupportedEncoding)) => {
            native_path_error(encoding, ffi::BridgeErrorCode::Unsupported)
        }
        Err(()) => native_path_error(encoding, ffi::BridgeErrorCode::Internal),
    }
}

fn native_path_error(
    encoding: ffi::NativePathEncoding,
    error: ffi::BridgeErrorCode,
) -> ffi::NativePathResult {
    ffi::NativePathResult {
        path: ffi::NativePath {
            encoding,
            data: Vec::new(),
        },
        error,
    }
}

fn type_ahead_step(
    names: Vec<String>,
    current_row: i32,
    prefix: String,
    typed: String,
) -> ffi::TypeAheadResult {
    let fallback_prefix = prefix.clone();
    match catch_boundary(|| {
        let step = tfx_core::type_ahead_step(&names, current_row, &prefix, &typed);
        ffi::TypeAheadResult {
            row: step.row,
            prefix: step.prefix,
            error: ffi::BridgeErrorCode::Ok,
        }
    }) {
        Ok(result) => result,
        Err(()) => ffi::TypeAheadResult {
            row: -1,
            prefix: fallback_prefix,
            error: ffi::BridgeErrorCode::Internal,
        },
    }
}

fn catch_boundary<T>(operation: impl FnOnce() -> T) -> Result<T, ()> {
    catch_unwind(AssertUnwindSafe(operation)).map_err(|_| ())
}

#[cfg(test)]
mod tests {
    use super::{catch_boundary, round_trip_native_path, type_ahead_step};
    use crate::ffi::{BridgeErrorCode, NativePath, NativePathEncoding};

    #[test]
    fn bridge_maps_core_result() {
        let result = type_ahead_step(
            vec!["alpha".to_owned(), "beta".to_owned()],
            -1,
            String::new(),
            "b".to_owned(),
        );
        assert_eq!(result.row, 1);
        assert_eq!(result.prefix, "b");
        assert_eq!(result.error, BridgeErrorCode::Ok);
    }

    #[test]
    fn boundary_catches_panics() {
        assert_eq!(catch_boundary(|| panic!("boundary test")), Err(()));
    }

    #[cfg(unix)]
    #[test]
    fn bridge_preserves_non_utf8_unix_path() {
        let data = b"/tmp/report-\xff.txt".to_vec();
        let result = round_trip_native_path(NativePath {
            encoding: NativePathEncoding::UnixBytes,
            data: data.clone(),
        });
        assert_eq!(result.error, BridgeErrorCode::Ok);
        assert_eq!(result.path.data, data);
    }

    #[test]
    fn bridge_maps_invalid_path_error() {
        let result = round_trip_native_path(NativePath {
            encoding: NativePathEncoding::UnixBytes,
            data: b"/tmp/a\0b".to_vec(),
        });
        assert_eq!(result.error, BridgeErrorCode::InvalidInput);
        assert!(result.path.data.is_empty());
    }
}
