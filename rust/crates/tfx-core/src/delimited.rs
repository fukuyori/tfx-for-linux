pub const MAX_DELIMITED_INPUT_BYTES: usize = 16 * 1024 * 1024;
pub const MAX_DELIMITED_ROWS: usize = 10_000;
pub const MAX_DELIMITED_COLUMNS: usize = 1_000;
pub const MAX_DELIMITED_CELLS: usize = 1_000_000;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DelimitedTable {
    pub rows: Vec<Vec<String>>,
    pub rows_truncated: bool,
    pub columns_truncated: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DelimitedError {
    InvalidInput,
}

struct Parser {
    table: DelimitedTable,
    row: Vec<String>,
    field: String,
    row_has_content: bool,
    max_rows: usize,
    max_columns: usize,
}

impl Parser {
    fn new(max_rows: usize, max_columns: usize) -> Self {
        Self {
            table: DelimitedTable {
                rows: Vec::new(),
                rows_truncated: false,
                columns_truncated: false,
            },
            row: Vec::new(),
            field: String::new(),
            row_has_content: false,
            max_rows,
            max_columns,
        }
    }

    fn end_field(&mut self) {
        self.row_has_content = self.row_has_content || !self.field.is_empty();
        if self.row.len() < self.max_columns {
            self.row.push(std::mem::take(&mut self.field));
        } else {
            self.table.columns_truncated = true;
            self.field.clear();
        }
    }

    fn end_row(&mut self) -> bool {
        self.end_field();
        let empty_row = !self.row_has_content && self.row.len() <= 1;
        if !empty_row {
            if self.table.rows.len() < self.max_rows {
                self.table.rows.push(std::mem::take(&mut self.row));
            } else {
                self.table.rows_truncated = true;
            }
        }
        self.row.clear();
        self.row_has_content = false;
        self.table.rows.len() >= self.max_rows && self.table.rows_truncated
    }
}

pub fn parse_delimited(
    content: &str,
    delimiter: char,
    max_rows: usize,
    max_columns: usize,
) -> Result<DelimitedTable, DelimitedError> {
    if content.len() > MAX_DELIMITED_INPUT_BYTES
        || max_rows == 0
        || max_rows > MAX_DELIMITED_ROWS
        || max_columns == 0
        || max_columns > MAX_DELIMITED_COLUMNS
        || max_rows
            .checked_mul(max_columns)
            .is_none_or(|cells| cells > MAX_DELIMITED_CELLS)
    {
        return Err(DelimitedError::InvalidInput);
    }

    let mut parser = Parser::new(max_rows, max_columns);
    let mut characters = content.chars().peekable();
    let mut in_quotes = false;

    while let Some(character) = characters.next() {
        if in_quotes {
            if character == '"' {
                if characters.peek() == Some(&'"') {
                    parser.field.push('"');
                    characters.next();
                } else {
                    in_quotes = false;
                }
            } else {
                parser.field.push(character);
            }
            continue;
        }

        if character == '"' && parser.field.is_empty() {
            in_quotes = true;
            parser.row_has_content = true;
        } else if character == delimiter {
            parser.end_field();
            parser.row_has_content = true;
        } else if character == '\n' || character == '\r' {
            if character == '\r' && characters.peek() == Some(&'\n') {
                characters.next();
            }
            if parser.end_row() {
                return Ok(parser.table);
            }
        } else {
            parser.field.push(character);
        }
    }

    if parser.row_has_content || !parser.field.is_empty() || !parser.row.is_empty() {
        parser.end_row();
    }
    Ok(parser.table)
}

#[cfg(test)]
mod tests {
    use super::{
        parse_delimited, DelimitedError, DelimitedTable, MAX_DELIMITED_CELLS,
        MAX_DELIMITED_COLUMNS, MAX_DELIMITED_INPUT_BYTES, MAX_DELIMITED_ROWS,
    };

    #[test]
    fn handles_quoting_and_embedded_newlines() {
        let table = parse_delimited(
            "name,note\n\"a,b\",\"line1\nline2\"\n\"say \"\"hi\"\"\",plain\n",
            ',',
            100,
            100,
        )
        .unwrap();
        assert_eq!(table.rows.len(), 3);
        assert_eq!(table.rows[1], ["a,b", "line1\nline2"]);
        assert_eq!(table.rows[2], ["say \"hi\"", "plain"]);
        assert!(!table.rows_truncated);
        assert!(!table.columns_truncated);
    }

    #[test]
    fn caps_rows_and_columns() {
        let content = (0..20)
            .map(|index| format!("row{index},x\n"))
            .collect::<String>();
        let rows = parse_delimited(&content, ',', 10, 100).unwrap();
        assert_eq!(rows.rows.len(), 10);
        assert!(rows.rows_truncated);

        let columns = parse_delimited("a,b,c,d,e\n", ',', 10, 3).unwrap();
        assert_eq!(columns.rows[0], ["a", "b", "c"]);
        assert!(columns.columns_truncated);
    }

    #[test]
    fn skips_empty_lines_but_keeps_present_empty_fields() {
        let table = parse_delimited("a,b\n\n\r\nc,d\n", ',', 100, 100).unwrap();
        assert_eq!(table.rows, [vec!["a", "b"], vec!["c", "d"]]);

        let delimiters = parse_delimited(",\n", ',', 100, 100).unwrap();
        assert_eq!(delimiters.rows, [vec!["", ""]]);

        let quoted_empty = parse_delimited("\"\"\n", ',', 100, 100).unwrap();
        assert_eq!(quoted_empty.rows, [vec![""]]);
    }

    #[test]
    fn preserves_newline_sequences_inside_quotes() {
        let table = parse_delimited("\"a\r\nb\",c\r\n", ',', 100, 100).unwrap();
        assert_eq!(table.rows, [vec!["a\r\nb", "c"]]);
    }

    #[test]
    fn rejects_invalid_limits_and_oversized_input() {
        assert_eq!(
            parse_delimited("a", ',', 0, 1),
            Err(DelimitedError::InvalidInput)
        );
        assert_eq!(
            parse_delimited("a", ',', 1, 0),
            Err(DelimitedError::InvalidInput)
        );
        assert_eq!(
            parse_delimited("a", ',', MAX_DELIMITED_ROWS + 1, 1),
            Err(DelimitedError::InvalidInput)
        );
        assert_eq!(
            parse_delimited("a", ',', 1, MAX_DELIMITED_COLUMNS + 1),
            Err(DelimitedError::InvalidInput)
        );
        assert_eq!(
            parse_delimited(
                "a",
                ',',
                MAX_DELIMITED_CELLS / MAX_DELIMITED_COLUMNS + 1,
                MAX_DELIMITED_COLUMNS
            ),
            Err(DelimitedError::InvalidInput)
        );

        let oversized = "a".repeat(MAX_DELIMITED_INPUT_BYTES + 1);
        assert_eq!(
            parse_delimited(&oversized, ',', 1, 1),
            Err(DelimitedError::InvalidInput)
        );
    }

    #[test]
    fn generated_unquoted_tables_round_trip() {
        for seed in 1..=256_u32 {
            let row_count = (seed as usize % 12) + 1;
            let column_count = ((seed.rotate_left(5) as usize) % 8) + 1;
            let mut content = String::new();
            let mut expected = Vec::new();

            for row in 0..row_count {
                let fields = (0..column_count)
                    .map(|column| format!("{seed:x}-{row}-{column}"))
                    .collect::<Vec<_>>();
                content.push_str(&fields.join(","));
                content.push('\n');
                expected.push(fields);
            }

            assert_eq!(
                parse_delimited(&content, ',', row_count, column_count),
                Ok(DelimitedTable {
                    rows: expected,
                    rows_truncated: false,
                    columns_truncated: false,
                })
            );
        }
    }
}
