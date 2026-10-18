//! 素材库：rusqlite 嵌入式数据库（开发大纲 material_lib/db.rs）。
//!
//! Phase 1 落地 DMC 绣线 / 珠子色卡表；面料、皮料等表在 Phase 2 随模板模块扩展。

use super::model::{Material, MaterialKind};
use crate::common::error::{CoreError, CoreResult};
use crate::cross_stitch::dmc_data::DMC_COLORS;
use crate::cross_stitch::perler_data::PERLER_COLORS;
use rusqlite::Connection;

pub struct MaterialDb {
    conn: Connection,
}

impl MaterialDb {
    /// 内存库 + 内置 DMC 绣线 + Artkal 拼豆数据（用户自定义素材落盘在 Phase 4）。
    pub fn open_in_memory() -> CoreResult<Self> {
        let conn = Connection::open_in_memory()?;
        let db = Self { conn };
        db.init_schema()?;
        db.seed_dmc()?;
        db.seed_perler()?;
        Ok(db)
    }

    fn init_schema(&self) -> CoreResult<()> {
        self.conn.execute_batch(
            r#"
            CREATE TABLE IF NOT EXISTS materials (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                code TEXT NOT NULL,
                name TEXT NOT NULL,
                rgb_r INTEGER, rgb_g INTEGER, rgb_b INTEGER,
                price_cents INTEGER,
                note TEXT,
                UNIQUE(kind, code)
            );
            CREATE INDEX IF NOT EXISTS idx_materials_code ON materials(code);
            "#,
        )?;
        Ok(())
    }

    fn seed_dmc(&self) -> CoreResult<()> {
        let mut stmt = self.conn.prepare(
            "INSERT OR IGNORE INTO materials (kind, code, name, rgb_r, rgb_g, rgb_b, note)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        )?;
        for c in DMC_COLORS {
            stmt.execute(rusqlite::params![
                MaterialKind::Yarn.as_str(),
                c.code,
                c.name,
                c.rgb[0] as i64,
                c.rgb[1] as i64,
                c.rgb[2] as i64,
                "builtin dmc subset"
            ])?;
        }
        Ok(())
    }

    fn seed_perler(&self) -> CoreResult<()> {
        let mut stmt = self.conn.prepare(
            "INSERT OR IGNORE INTO materials (kind, code, name, rgb_r, rgb_g, rgb_b, note)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        )?;
        for c in PERLER_COLORS {
            stmt.execute(rusqlite::params![
                MaterialKind::Perler.as_str(),
                c.code,
                c.name,
                c.rgb[0] as i64,
                c.rgb[1] as i64,
                c.rgb[2] as i64,
                "artkal s 5mm perler beads"
            ])?;
        }
        Ok(())
    }

    /// 按色号查询。
    pub fn find_by_code(&self, code: &str) -> CoreResult<Option<Material>> {
        let mut stmt = self.conn.prepare(
            "SELECT kind, code, name, rgb_r, rgb_g, rgb_b, price_cents, note FROM materials WHERE code = ?1",
        )?;
        let mut rows = stmt.query(rusqlite::params![code])?;
        if let Some(row) = rows.next()? {
            Ok(Some(row_to_material(row)?))
        } else {
            Ok(None)
        }
    }

    /// 某大类素材数量。
    pub fn count(&self, kind: MaterialKind) -> CoreResult<usize> {
        let n: i64 = self.conn.query_row(
            "SELECT COUNT(*) FROM materials WHERE kind = ?1",
            rusqlite::params![kind.as_str()],
            |r| r.get(0),
        )?;
        Ok(n as usize)
    }

    /// 枚举某大类全部素材（按色号排序），用于色卡展示。
    pub fn list(&self, kind: MaterialKind) -> CoreResult<Vec<Material>> {
        let mut stmt = self.conn.prepare(
            "SELECT kind, code, name, rgb_r, rgb_g, rgb_b, price_cents, note \
             FROM materials WHERE kind = ?1 ORDER BY code",
        )?;
        let mut rows = stmt.query(rusqlite::params![kind.as_str()])?;
        let mut out = Vec::new();
        while let Some(row) = rows.next()? {
            out.push(row_to_material(row)?);
        }
        Ok(out)
    }

    /// 插入自定义素材。
    pub fn insert(&self, m: &Material) -> CoreResult<()> {
        self.conn.execute(
            "INSERT OR REPLACE INTO materials (kind, code, name, rgb_r, rgb_g, rgb_b, price_cents, note)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
            rusqlite::params![
                m.kind.as_str(),
                m.code,
                m.name,
                m.rgb.map(|c| c[0] as i64),
                m.rgb.map(|c| c[1] as i64),
                m.rgb.map(|c| c[2] as i64),
                m.price_cents.map(|p| p as i64),
                m.note,
            ],
        )?;
        Ok(())
    }
}

fn row_to_material(row: &rusqlite::Row<'_>) -> CoreResult<Material> {
    let kind_str: String = row.get(0)?;
    let kind = match kind_str.as_str() {
        "fabric" => MaterialKind::Fabric,
        "yarn" => MaterialKind::Yarn,
        "leather" => MaterialKind::Leather,
        "button" => MaterialKind::Button,
        "zipper" => MaterialKind::Zipper,
        "bead" => MaterialKind::Bead,
        "perler" => MaterialKind::Perler,
        other => return Err(CoreError::Db(format!("unknown material kind {other}"))),
    };
    let rgb = match (
        row.get::<_, Option<i64>>(3)?,
        row.get::<_, Option<i64>>(4)?,
        row.get::<_, Option<i64>>(5)?,
    ) {
        (Some(r), Some(g), Some(b)) => Some([r as u8, g as u8, b as u8]),
        _ => None,
    };
    Ok(Material {
        kind,
        code: row.get(1)?,
        name: row.get(2)?,
        rgb,
        price_cents: row.get::<_, Option<i64>>(6)?.map(|p| p as u32),
        note: row.get(7)?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn db_seeds_dmc_and_queries() {
        let db = MaterialDb::open_in_memory().unwrap();
        assert!(db.count(MaterialKind::Yarn).unwrap() >= 90);
        let m = db.find_by_code("321").unwrap().expect("DMC 321 exists");
        assert_eq!(m.name, "Red");
        assert_eq!(m.rgb, Some([199, 15, 46]));
        assert!(db.find_by_code("NOPE").unwrap().is_none());
    }

    #[test]
    fn db_insert_custom() {
        let db = MaterialDb::open_in_memory().unwrap();
        db.insert(&Material {
            kind: MaterialKind::Fabric,
            code: "LINEN-140".to_string(),
            name: "Linen 140g".to_string(),
            rgb: Some([200, 180, 160]),
            price_cents: Some(1500),
            note: "test".to_string(),
        })
        .unwrap();
        let m = db.find_by_code("LINEN-140").unwrap().unwrap();
        assert_eq!(m.kind, MaterialKind::Fabric);
        assert_eq!(db.count(MaterialKind::Fabric).unwrap(), 1);
    }
}
