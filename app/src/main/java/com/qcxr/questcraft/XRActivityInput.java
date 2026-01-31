package com.qcxr.questcraft;

public class XRActivityInput {
    static void clickScreenAtPosition(int x, int y) {
        System.out.println("Clicking screen at position: " + x + ", " + y);
        MainActivity.webView.performContextClick(x, y);
    }
}
