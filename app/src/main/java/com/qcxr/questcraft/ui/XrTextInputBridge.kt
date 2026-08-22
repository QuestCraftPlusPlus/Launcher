package com.qcxr.questcraft.ui

import android.os.Handler
import android.os.Looper
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.input.TextFieldValue
import com.qcxr.questcraft.JniBridge

object XrTextInputBridge {
    private val mainHandler = Handler(Looper.getMainLooper())

    private data class ActiveField(
        val getValue: () -> TextFieldValue,
        val setValue: (TextFieldValue) -> Unit,
    )

    @Volatile private var active: ActiveField? = null

    internal fun register(getValue: () -> TextFieldValue, setValue: (TextFieldValue) -> Unit) {
        active = ActiveField(getValue, setValue)
        JniBridge.showKeyboard()
    }

    internal fun unregister(getValue: () -> TextFieldValue) {
        if (active?.getValue === getValue) {
            active = null
            JniBridge.hideKeyboard()
        }
    }

    @JvmStatic
    fun sendText(text: String) {
        mainHandler.post {
            val field = active ?: return@post
            val v = field.getValue()
            val newText = v.text.replaceRange(v.selection.start, v.selection.end, text)
            val newCursor = v.selection.start + text.length
            field.setValue(TextFieldValue(newText, TextRange(newCursor)))
        }
    }

    @JvmStatic
    fun deleteCharacter() {
        mainHandler.post {
            val field = active ?: return@post
            val v = field.getValue()
            if (v.selection.start != v.selection.end) {
                val newText = v.text.removeRange(v.selection.start, v.selection.end)
                field.setValue(TextFieldValue(newText, TextRange(v.selection.start)))
            } else if (v.selection.start > 0) {
                val cut = v.selection.start - 1
                val newText = v.text.removeRange(cut, v.selection.start)
                field.setValue(TextFieldValue(newText, TextRange(cut)))
            }
        }
    }
}