package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.R
import com.qcxr.questcraft.ui.theme.*

@Composable
fun BottomControlBar(selectedInstance: Instance?, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 24.dp)
            .background(SurfaceDark, RoundedCornerShape(8.dp))
            .padding(24.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column {
            Text(text = stringResource(R.string.selected_instance).uppercase(), color = TextSecondary, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            Text(text = selectedInstance?.jLibInstance?.instanceName ?: "NONE", color = TextPrimary, fontSize = 22.sp, fontWeight = FontWeight.Black)
        }
        Spacer(modifier = Modifier.width(32.dp))
        Column {
            Text(text = stringResource(R.string.version).uppercase(), color = TextSecondary, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            Text(text = selectedInstance?.jLibInstance?.versionName ?: "-", color = TextPrimary, fontSize = 22.sp)
        }
        Spacer(modifier = Modifier.width(32.dp))
        Column {
            Text(text = stringResource(R.string.loader).uppercase(), color = TextSecondary, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            // TODO: Update to loaderName
            Text(text = selectedInstance?.jLibInstance?.versionType ?: "-", color = TextPrimary, fontSize = 22.sp)
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        // Action buttons
        Row {
            Box(modifier = Modifier.size(56.dp).background(CardBackground, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
                Box(modifier = Modifier.size(24.dp).background(Color.Gray)) // Pencil icon
            }
            Spacer(modifier = Modifier.width(12.dp))
            Box(modifier = Modifier.size(56.dp).background(CardBackground, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
                Box(modifier = Modifier.size(24.dp).background(Color.Gray)) // Trash icon
            }
        }
        
        Spacer(modifier = Modifier.width(24.dp))
        
        Button(
            onClick = {},
            modifier = Modifier.height(52.dp).width(160.dp),
            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
            shape = RoundedCornerShape(4.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(modifier = Modifier.size(16.dp).background(Color.White)) // Play icon
                Spacer(modifier = Modifier.width(8.dp))
                Text(text = stringResource(R.string.launch).uppercase(), color = Color.White, fontWeight = FontWeight.Bold)
            }
        }
    }
}

@Preview(widthDp = 1280)
@Composable
fun BottomControlBarPreview() {
    QuestCraftTheme {
        BottomControlBar(selectedInstance = null)
    }
}