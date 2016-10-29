package jp.co.infoseek.hp.arton.rjb;

public class CallbackTest {
    public interface Callback {
        String method(long lval, short s, int n, double d, String str);
    }

    public static String callCallback(Callback cb) {
        return cb.method(1234L, (short)1234, 1234, 1234.5, "1234");
    }
}

