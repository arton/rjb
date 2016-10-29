準備
・あらかじめ環境変数にJAVA_HOMEを設定しておいてください。
・この場合、JAVA_HOMEは、J2SDKのインストールディレクトリの必要があります。
・あらかじめ環境変数PATHに$JAVA_HOME/binを設定しておいてください。
・Windowsの場合、PATHには%PATH%;%JAVA_HOME%binを設定することになります。
・ruby1.8以降が実行できるようにPATHを設定しておいてください。

インストール方法
1. unzip rjb-*
2. cd rjb-*
3. ruby setup.rb config
4. ruby setup.rb setup
5. sudo ruby setup.rb install
   Windowsでは、ほとんどの場合最初のsudoは不要です。「ほとんどの場合」に該当しない場合は何が必要かはわかっているはずですので説明は省略します。

実行時
・あらかじめ環境変数にJAVA_HOMEを設定しておいてください。
・この場合、JAVA_HOMEは、J2SDKのインストールディレクトリの必要があります。
・Linuxに関してはLD_LIBRARY_PATHに、java2の共有オブジェクトディレクトリを設定しておく必要があります。

テストした環境
Windows2000 SP4-ruby1.8.2-j2se1.5.0, Solaris9-ruby1.8.0-j2se1.4.2, Linux 2.4.26-ruby-1.8.1-j2se1.4.2

連絡先
artonx@yahoo.co.jp
https://www.artonx.org/collabo/backyard/?RjbQandA (記入時にはdiaryへツッコミを入れてください）

