なるほど。`Sheen_LTC.dds` はパストレーシングにおけるImportance Sampling用であり、今回の目的であるリアルタイムArea LightのLTC評価には不要という整理ですね。

それなら、初期仕様をさらにシンプルにできます。

## 修正後の出力仕様

標準出力は次の2種類とします。

| ファイル             | 用途                      | DDS形式   |
| ---------------- | ----------------------- | ------- |
| `GGX_LTCMat.dds` | GGXのLTC逆変換行列            | RGBA16F |
| `GGX_LTCAmp.dds` | GGXのAmplitude/Fresnel係数 | RG16F   |

`Sheen_LTC.dds` は初期開発対象から除外します。

ただし、Sheen BRDF自体を将来のフィッティング対象から除外する必要はありません。 リアルタイムArea Light用のSheen LTCが必要になれば、通常のMatrix + Amplitude形式で生成できます。

## 修正後のロードマップ

| Phase | 内容                                 |
| ----- | ---------------------------------- |
| 0     | C++20、CMake、CLI、DDS Reader         |
| 1     | GGX BRDF、LTC数学モデル                  |
| 2     | Nelder–MeadによるCPU Fitting          |
| 3     | GGX LUT生成、マルチスレッド                  |
| 4     | DDS Writer、Unreal Engine互換パッキング    |
| 5     | Unreal Engineの既存DDSとの数値比較          |
| 6     | Beckmann、Disney Diffuseなどの汎用BRDF対応 |
| 7     | 高品質フィッティング、数値積分検証、性能最適化            |

### 設計上の重要な区別

LTCには主に2つの用途があります。

- リアルタイムArea Light積分：LTCでBRDFを近似し、多角形光源からの照明を効率的に積分する。
- パストレーシングのImportance Sampling：LTC分布を使ってサンプル方向を生成する。

基礎となるLTCの数学は共通ですが、必要な係数、正規化、Amplitudeの扱い、テクスチャのパッキングは用途によって異なり得ます。

今回のツールでは、前者に集中します。

\*\*最終的な初期スコープは「C++20 / CPU / CLI / DDS / GGX + 汎用BRDF対応 / リアルタイムArea Light向け」\*\*で確定してよいと思います。

この構成なら、パストレーシング固有のImportance Sampling機能を実装せず、LTCフィッティングと照明積分の精度向上に集中できます。
