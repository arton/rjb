import jp.co.infoseek.hp.arton.rjb.Test;

public class JTest {
    static final String[] rs = new String[] { "a", "b", "c", "d", "e",
                                              "f", "g", "h", "i" };
        static final Integer[] ri = new Integer[] { new Integer(1),
                                                new Integer(2),
                                                new Integer(3),
                                                new Integer(4),
                                                new Integer(5),
                                                new Integer(6),
                                                new Integer(7),
                                                new Integer(8),
                                                new Integer(9)
        };
    public static void main(String[] args) {
        Test t = new Test();
        String[] a = t.joinStringArray(new String[][] { 
 	    {"a", "b", "c"}, {"d", "e", "f"}, {"g", "h", "i"} });
        System.out.println(a.length);
        for (int i = 0; i < a.length; i++) {
            System.out.print(a[i]);
            if (rs[i] != a[i]) {
                System.out.println("bad result !");
                System.exit(1);
            }
        }
        System.out.println("");
        Integer[] ai = t.joinIntArray(new int[][] {
                  { 1, 2, 3, }, { 4, 5, 6, }, { 7, 8, 9, } });
        System.out.println(ai.length);
        for (int i = 0; i < ai.length; i++) {
            System.out.print(ai[i]);
            if (!ri[i].equals(ai[i])) {
                System.out.println("bad result !");
                System.exit(1);
            }
        }
        System.out.println("");
    }
}

