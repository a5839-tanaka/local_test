次に示す３つのプロセスまたはシェルは root 権限で動作させる必要があるため、
パーミッションに "s" を設定しないこと

  pvinit.sh  rwxr-xr-x
  pvinit     rwxr-xr-x
  pvmanage   rwxr-xr-x

シリアルデバイスにアクセスする下記のプロセスは、dialout グループに所属の
プロセスとして動作させるため、所属グループを dialout とし、スティッキー
ビット "s" を設定する

  pcsshin1   rwsr-sr-x  pvsystem dialout
  pcsdaih1   rwsrwsr-x  pvsystem dialout
  pcstmei1   rwsrwsr-x  pvsystem dialout
    :
    :
  penemoni   rwsrwsr-x  pvsystem dialout
  omronkm1   rwsrwsr-x  pvsystem dialout
  modbusr1   rwsrwsr-x  pvsystem dialout
  dcmonit1   rwsrwsr-x  pvsystem dialout
  pvdspsc1   rwsrwsr-x  pvsystem dialout
    :
    :

上記以外のプロセスは pvsystem ユーザで動作させるため "s" を設定する

  repoedit   rwsr-sr-x  pvsystem pvsystem
  pvftpsnd   rwsr-sr-x  pvsystem pvsystem
  anopvrcv   rwsrwsr-x  pvsystem dialout
  anopvsnd   rwsrwsr-x  pvsystem dialout
    :
    :

