package jp.co.infoseek.hp.arton.rjb;
public class TwoCaller {
    public String[] foo(Two t) {
        String[] ret = new String[2];
        ret[0] = t.method1();
        ret[1] = t.method2();
        return ret;
    }
}
