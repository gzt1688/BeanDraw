//! 导出工具：CSV（带 UTF-8 BOM，Excel 兼容）。

use crate::common::error::CoreResult;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;

/// 创建带 BOM 的 CSV 写入器。
pub fn open_material_csv(path: &Path) -> CoreResult<BufWriter<File>> {
    let mut w = BufWriter::new(File::create(path)?);
    w.write_all(&[0xEF, 0xBB, 0xBF])?;
    Ok(w)
}

/// 通用 CSV 行写入（字段内逗号/引号自动转义）。
pub fn write_row<W: Write>(w: &mut W, fields: &[&str]) -> CoreResult<()> {
    let escaped: Vec<String> = fields
        .iter()
        .map(|f| {
            if f.contains(',') || f.contains('"') || f.contains('\n') {
                format!("\"{}\"", f.replace('"', "\"\""))
            } else {
                f.to_string()
            }
        })
        .collect();
    writeln!(w, "{}", escaped.join(","))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bom_written() {
        let dir = std::env::temp_dir();
        let path = dir.join("craftmaster_csv_helper_test.csv");
        let mut w = open_material_csv(&path).unwrap();
        write_row(&mut w, &["a", "b"]).unwrap();
        w.flush().unwrap();
        let bytes = std::fs::read(&path).unwrap();
        assert!(bytes.starts_with(&[0xEF, 0xBB, 0xBF]));
        assert!(String::from_utf8_lossy(&bytes[3..]).contains("a,b"));
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn escaping() {
        let mut buf: Vec<u8> = Vec::new();
        write_row(&mut buf, &["plain", "with,comma", "with\"quote"]).unwrap();
        let s = String::from_utf8(buf).unwrap();
        assert!(s.contains("\"with,comma\""));
        assert!(s.contains("\"with\"\"quote\""));
    }
}
