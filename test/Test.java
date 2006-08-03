package jp.co.infoseek.hp.arton.rjb;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Comparator;
import java.math.BigDecimal;

public class Test {
    public String concat(Iterator i) {
	StringBuffer sb = new StringBuffer();
	for (; i.hasNext(); ) {
	    sb.append(i.next());
	}
	return new String(sb);
    }
    public int check(Comparator c, int x, int y) {
	return c.compare(new Integer(x), new Integer(y));
    }
    public String[][] getStringArrayOfArrays() {
	return new String[][] { { "abc", "def" }, { "123", "456" } };
    }

    public int[][] getIntArrayOfArrays() {
	return new int[][] { { 1,2,3 }, { 4, 5, 6} };
    }

    public Object[][][] getMixedArray() {
	return new Object[][][] {
 		{
			{12, "test", new Integer(15), new BigDecimal("1234.567")}, {}
		},
		{
			{"a string","another string"}, {1,2,3}, {4,5,6}
		},
		{
		},
	};
    }

    public String[][][][] getSizedArray() {
	String[][][][] sizedArray = new String[1][2][3][4];
	sizedArray[0][1][2][3]="find me";
	return sizedArray;
    }

    public String[] joinStringArray(String[][] aa) {
        ArrayList<String> list = new ArrayList<String>();
        for (int i = 0; i < aa.length; i++) {
            for (int j = 0; j < aa[i].length; j++) { 
                list.add(aa[i][j]);
            }
        }
        return list.toArray(new String[list.size()]);
    }
    public Integer[] joinIntArray(int[][] aa) {
        ArrayList<Integer> list = new ArrayList<Integer>();
        for (int i = 0; i < aa.length; i++) {
            for (int j = 0; j < aa[i].length; j++) { 
                list.add(aa[i][j]);
            }
        }
        return list.toArray(new Integer[list.size()]);
    }
    public int[][][] throughIntArray(int[][][] a) {
        return a;
    }
}
