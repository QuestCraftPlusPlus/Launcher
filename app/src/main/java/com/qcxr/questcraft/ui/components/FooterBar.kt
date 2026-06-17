package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.ui.theme.*

@Composable
fun FooterBar(modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(text = ">_ VIEW CONSOLE", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
        Spacer(modifier = Modifier.width(32.dp))
        
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = "CPU ", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Text(text = "23%", color = AccentGreen, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.width(16.dp))
            Text(text = "RAM ", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Text(text = "5.2 GB", color = AccentGreen, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.width(16.dp))
            Text(text = "FPS ", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Text(text = "72", color = AccentGreen, fontSize = 11.sp, fontWeight = FontWeight.Bold)
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Wifi/Network icon placeholder
            Box(modifier = Modifier.size(14.dp).background(TextSecondary))
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = "QUEST-3_EXT", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.width(16.dp))
            Text(text = "v3.1.0-STABLE", color = TextSecondary, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.width(12.dp))
            // Help icon placeholder
            Box(modifier = Modifier.size(16.dp).background(TextSecondary, RoundedCornerShape(8.dp)), contentAlignment = Alignment.Center) {
                Text("?", color = BackgroundDark, fontSize = 10.sp, fontWeight = FontWeight.Black)
            }
        }
    }
}

@Preview(widthDp = 1280)
@Composable
fun FooterBarPreview() {
    QuestCraftTheme {
        FooterBar()
    }
}