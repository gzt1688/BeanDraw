//! 工具示例：导出 DMC 色卡子集为 JSON（供 Qt 侧 / 文档使用）。
//!
//! 用法：
//! ```sh
//! cargo run --example dump_dmc > ../resources/dmc_color.json
//! ```

use craftmaster_core::cross_stitch::dmc_data::DMC_COLORS;

fn main() {
    let entries: Vec<String> = DMC_COLORS
        .iter()
        .map(|c| {
            format!(
                "  {{\"code\": \"{}\", \"name\": \"{}\", \"rgb\": [{}, {}, {}]}}",
                c.code, c.name, c.rgb[0], c.rgb[1], c.rgb[2]
            )
        })
        .collect();
    println!("{{");
    println!("  \"description\": \"DMC floss color subset (development dataset, ~{} colors). Not commercial-grade; validate sources before release.\",", DMC_COLORS.len());
    println!("  \"colors\": [");
    println!("{}", entries.join(",\n"));
    println!("  ]");
    println!("}}");
}
