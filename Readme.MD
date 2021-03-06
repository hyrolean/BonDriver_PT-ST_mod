# BonDriver_PT-ST MOD

**※このリポジトリは、クローズ予定です。（[こちらのリポジトリ](https://github.com/hyrolean/BonDriver_PTx-ST_mod)に統合されました。）**

## オリジナルのBonDriver_PT(人柱版３)との使用する上での相違点

- チャンネルファイル.ChSet.txtファイルに -/+ を値欄に記述することにより、直近に登録したチャンネルの値を参照できるようになりました。
  ```
  - … 直近に登録したチャンネルの値と同値
  + … 直近に登録したチャンネルの値に+1加算したもの
  ```
  この機能を上手く使いこなすことにより、チャンネルファイル.ChSet.txtを編集する際の通し番号を記述することにより発生する煩わしいコストを削減出来ます。
  - [mod2 BSAlmighty+DeltaHz+DefSpace fixes](https://github.com/hyrolean/BonDriver_PT-ST_mod/releases/tag/mod2)
    (Described by 2021/02/12 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

- 既定のチャンネル情報を編集することにより、チャンネル情報構築の簡略化に対応。
  チャンネルファイル.ChSet.txtが読み込まれなかった場合は、BonDriver_PT-ST.iniのDefSpaceセクションの記述内容に応じてチャンネル情報を自動構築します。
  特殊な受信環境でなければ、.ChSet.txtファイルの複雑怪奇な編集作業を実質まったく行う必要がなくなり、チャンネル情報書き換えによる煩わしいコストを省けます。
  - [mod2α2 BSAlmighty+DeltaHz+DefSpace fixes](https://github.com/hyrolean/BonDriver_PT-ST_mod/releases/tag/mod2_alpha2)
    (Described by 2021/02/04 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
  
  **※既定のチャンネル情報を有効にするには、いままで利用していた.ChSet.txtファイルを抹消し、代替に添付のBonDriver_PT-ST.iniファイルをBonDriverと同じフォルダに置いてDefSpaceセクションをチューナー環境に応じて適切に編集してみて下さい。**

- ヘタってきたチューナー用に周波数を微調整する機能を追加しました。
  チャンネルファイルの"PTxとしてのチャンネル"部分にch番号±δで調整できます。
  (単位は、1/7MHz。実際の周波数より2MHz上方修正する場合は、ch番号+14と書く。)
  たとえば、CATVのC37チャンネルの周波数を1/7MHz下方修正する場合は、添付のBonDriver_PT-T.ChSet.txtの81行目を下記の様に記述することにより、
  ```
  C37Ch	1	24	36-1	0
  ```
  実際のC37チャンネルの周波数より1/7MHz下方(-143kHz)修正されます。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT-ST_mod/releases/tag/mod) 
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

- BonDriverのdllファイル名と同じ名前の拡張子.ChSet.txtファイルが存在する場合、そちらを先に優先してチャンネルファイルとして読み込む仕様に変更しました。
  たとえば、
  ```
  BonDriver_PT-T.ChSet.txt
  BonDriver_PT-T1.ChSet.txt
  BonDriver_PT-T1.dll
  ```
  の３ファイルが同一フォルダ上に存在する状態でBonDriver_PT-T1.dllがアプリケーション側から読み込まれた場合は、BonDriver_PT-T.ChSet.txtは読み込まれずに代替にBonDriver_PT-T1.ChSet.txtがチャンネルファイルとして読み込まれます。
  ヘタったチューナーの周波数を微調整したい場合や、地上波とCATVに分けたい場合などに個別のチャンネルファイルとして取扱う目的で利用できるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT-ST_mod/releases/tag/mod) 
    (Described by 2020/01/08 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

- BonDriver_PT-S.ChSet.txtのTSID項目に0～7の値を指定するとチューナーからTSIDを抜き取って下位3ビットがその値と一致するTSIDを持つストリームを自動選択します。
  トランスポンダの再編などによりチャンネル情報が変更された場合にチャンネルファイルのTSIDを実質一々書き換える必要がなくなりアプリケーション側のチャンネルスキャンだけで事が足りるようになります。
  - [mod BSAlmighty+DeltaHz fixes](https://github.com/hyrolean/BonDriver_PT-ST_mod/releases/tag/mod) 
    (Described by 2020/01/07 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
  
  **※機能を有効にするには、添付のBonDriver_PT-S.ChSet.txtを差し替えてください。また、添付のBonDriver_PT-S.ChSet.txtを差し替えた結果、EDCBでのチャンネルのスキャン時に抜けが発生する場合は、下記のようにEDCBのディレクトリ上に存在する設定ファイルBonCtrl.iniのServiceChkTimeOutの値を15くらいに上げて再スキャンしてみてください。**
  ```
  ;チャンネルスキャン時の動作設定
  [CHSCAN]
  ServiceChkTimeOut=15
  ```

- BonDriverのバイナリファイルはS用もT用も同一です。
  名前が「BonDriver_PT-T.dll」あるいは「BonDriver_PT-Tx.dll」の場合はT用として、「BonDriver_PT-S.dll」あるいは「BonDriver_PT-Sx.dll」の場合はS用として動作するので、dllファイルをコピーし、名前を変更して使用して下さい。

- iniファイルの中に「PTver」の項目が増えています。
  このBonDriverをPT2で使用する人は「2」を、PT1で使用する人は「1」を指定してください。
  この値は実質ほぼ利用されていないので、EDCBで使用する場合以外は多分変更する必要はありません。
  (EDCBでは設定ファイルの名称にBonDriverのファイル名だけでなくこの値も使用している為、ここが間違っていると既存の設定ファイル(XXXX.ChSet4.txt)が読み込めなくなります。)
