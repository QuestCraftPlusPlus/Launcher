package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.ui.theme.*

@Composable
fun FooterBar(modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(text = ">_ VIEW CONSOLE", color = TextSecondary, fontSize = 14.sp)
        Spacer(modifier = Modifier.width(24.dp))
        Text(text = "CPU 23%    RAM 5.2 GB    FPS 72", color = AccentGreen, fontSize = 14.sp)
        Spacer(modifier = Modifier.weight(1f))
        Text(text = "QUEST-3_EXT | v3.1.0-STABLE ?", color = TextSecondary, fontSize = 14.sp)
    }
}

@Preview(widthDp = 1280)
@Composable
fun FooterBarPreview() {
    QuestCraftTheme {
        FooterBar()
    }
}