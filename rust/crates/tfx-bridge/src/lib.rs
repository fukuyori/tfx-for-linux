use std::panic::{catch_unwind, AssertUnwindSafe};

#[cxx::bridge(namespace = "tfx::rust_bridge")]
mod ffi {
    #[derive(Debug)]
    enum BridgeErrorCode {
        Ok = 0,
        Internal = 1,
    }

    struct TypeAheadResult {
        row: i32,
        prefix: String,
        error: BridgeErrorCode,
    }

    extern "Rust" {
        fn type_ahead_step(
            names: Vec<String>,
            current_row: i32,
            prefix: String,
            typed: String,
        ) -> TypeAheadResult;
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
    use super::{catch_boundary, type_ahead_step};
    use crate::ffi::BridgeErrorCode;

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
}
