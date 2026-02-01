package com.qcxr.questcraft;

public class XRActivityInput {
    static void clickScreenAtPosition(int x, int y) {
        System.out.println("Clicking screen at position: " + x + ", " + y);
        if (MainActivity.webView != null) {
            MainActivity.webView.performContextClick(x, y);
        }
    }
}
