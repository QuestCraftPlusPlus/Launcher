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
import com.qcxr.questcraft.MainActivity
import com.qcxr.questcraft.R
import com.qcxr.questcraft.ui.theme.*
import org.angelauramc.judgelib.JudgeLibAPI
import org.angelauramc.judgelib.launcher.AndroidJavaLauncher
import org.angelauramc.judgelib.util.json.auth.JudgeLibAccount

@Composable
fun BottomControlBar(
    selectedInstance: Instance?,
    selectedAccount: JudgeLibAccount?,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 16.dp)
            .background(SurfaceDark, RoundedCornerShape(4.dp))
            .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
            .padding(horizontal = 24.dp, vertical = 20.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column {
            Text(text = stringResource(R.string.selected_instance), color = TextSecondary, fontSize = 10.sp, fontWeight = FontWeight.Bold)
            Text(text = selectedInstance?.jLibInstance?.instanceName?.uppercase() ?: "NONE", color = TextPrimary, fontSize = 18.sp, fontWeight = FontWeight.Black)
        }
        Spacer(modifier = Modifier.width(48.dp))
        Column {
            Text(text = stringResource(R.string.version), color = TextSecondary, fontSize = 10.sp, fontWeight = FontWeight.Bold)
            Text(text = selectedInstance?.jLibInstance?.versionName ?: "-", color = TextSecondary, fontSize = 18.sp)
        }
        Spacer(modifier = Modifier.width(32.dp))
        Column {
            Text(text = stringResource(R.string.loader), color = TextSecondary, fontSize = 10.sp, fontWeight = FontWeight.Bold)
            Text(text = selectedInstance?.jLibInstance?.versionType ?: "-", color = TextSecondary, fontSize = 18.sp)
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        // Action buttons
        Row {
            Box(modifier = Modifier.size(48.dp).background(CardBackground, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
                // Pencil icon
                Box(modifier = Modifier.size(16.dp).background(TextSecondary))
            }
            Spacer(modifier = Modifier.width(8.dp))
            Box(modifier = Modifier.size(48.dp).background(CardBackground, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
                // Trash icon
                Box(modifier = Modifier.size(16.dp).background(TextSecondary))
            }
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        Button(
            onClick = {
                // TODO: Implement custom args
                MainActivity.androidJavaLauncher.prepareGame(
                    selectedInstance?.jLibInstance,
                    selectedAccount,
                    null,
                    null
                )
                MainActivity.androidJavaLauncher.launchGame()
            },
            modifier = Modifier.height(48.dp).width(160.dp),
            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
            shape = RoundedCornerShape(4.dp),
            contentPadding = PaddingValues(0.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                // Play icon
                Box(modifier = Modifier.size(12.dp).background(Color.White))
                Spacer(modifier = Modifier.width(12.dp))
                Text(text = stringResource(R.string.launch), color = Color.White, fontWeight = FontWeight.Black, fontSize = 16.sp)
            }
        }
    }
}

@Preview(widthDp = 1280)
@Composable
fun BottomControlBarPreview() {
    QuestCraftTheme {
        BottomControlBar(selectedInstance = null, selectedAccount = null)
    }
}