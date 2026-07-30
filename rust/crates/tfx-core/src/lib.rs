use casefold::simple_fold;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TypeAheadStep {
    pub row: i32,
    pub prefix: String,
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
    use super::{type_ahead_step, TypeAheadStep};

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
}
