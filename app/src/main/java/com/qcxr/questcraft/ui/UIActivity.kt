package com.qcxr.questcraft.ui

import android.content.Context
import android.os.Bundle
import android.view.View
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.ui.platform.ComposeView
import com.qcxr.questcraft.ui.theme.QuestCraftTheme

class UIActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            QuestCraftTheme {
                QuestLauncherScreen()
            }
        }
    }

    companion object {
        @JvmStatic
        fun createView(context: Context): View {
            return ComposeView(context).apply {
                setContent {
                    QuestCraftTheme {
                        QuestLauncherScreen()
                    }
                }
            }
        }
    }
}