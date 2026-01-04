# 9. 从 IR 到 MIPS：后端常见冗余与 Peephole

后端冗余通常来自两类：

1. 地址生成策略保守（每次都 `la`/`addiu`）
2. IR 临时过多（变成连续 `move`）

可以先做“最稳妥”的 peephole：

- `move r, r` 删掉
- 本地数组 + 常量下标：直接生成 `lw/sw off($fp)`，避免 `addiu tmp,$fp,off; lw/sw 0(tmp)`
- 传参前的临时搬运尽量折叠（如果 IR 侧做了 Cleanup，后端会自然变好）
