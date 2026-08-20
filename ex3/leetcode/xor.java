import java.util.Scanner;

class Result {

    public static int maximizingXor(int l, int r) {
        int max = 0;

        for (int i = l; i <= r; i++) {
            for (int j = i; j <= r; j++) {
                max = Math.max(max, i ^ j);
            }
        }

        return max;
    }
}
