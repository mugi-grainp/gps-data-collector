# gps-data-collector - GPS測位・データ回収システム

## 概要

M5Stack と M5Atom + GPSユニットを利用したGPS測位ガジェット、および記録データ回収システム。

- 子機（測位ユニット）
    - M5Atom + GPSユニットにより、位置情報を取得し、microSDカードに一時的に蓄積する。
- 親機（回収ユニット）
    - M5Stackを利用してWi-Fiアクセスポイントを構築し、子機が接続してきた際に送信する位置情報記録を受信してmicroSDカードに記録する。

## 概念図

```
                                                    --------
            -----------                             | 子機 |
            |         |    接続・GPSデータ送信      --------
            |   親機  |      <=============
            |         |                             --------
            -----------                             | 子機 |
                                                    --------
Wi-Fiアクセスポイントとして動作                 GPSユニットを搭載
```

## ユースケース

- 子機を複数の車両に置いて位置情報を収集蓄積する。
- 拠点に設置した親機に接続することで、複数の子機のデータをmicroSDカードを抜き差しすることなく集約できる。

## システム構成

### 親機

- プログラムファイル: DataReceiver.ino
- M5Stackに対してインストールする。
- 子機とWi-Fiで接続されているとき、子機から送信されたデータをmicroSDカードに蓄積する。
    - ファイル名は1回送受信ごとに、「gps_log_XXXXX(数字連番).csv」という名のファイルに保存される。
    - 数字連番は、親機が起動されている間は番号が増え続ける。電源が切れて再起動すると番号が1に戻る。

### 子機

- プログラムファイル: GPSDongle.ino
- GPSユニットを接続したM5Atomに対してインストールする。
    - [ATOM GPS Development Kit (M8030-KT)](https://m5stack.com/products/atom-gps-kit-m8030-kt)
- インストール前に、プログラムファイル内の子機識別IDを機体ごとに設定しなおす。
- 親機とWi-Fi接続されると、M5AtomのLEDが青く点灯する。
- 親機とWi-Fi接続されている状態でM5Atomのボタンを押すと、親機へのデータ送信を試みる。
    - 送信成功したら緑色に点灯する。
    - 送信失敗したら赤色に点灯する。