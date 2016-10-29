package jp.co.infoseek.hp.arton.rjb;

public class JarTest2 extends JarTest {
    public String add(String a, String b) {
        return super.add(a, b) + " extended";
    }
}
