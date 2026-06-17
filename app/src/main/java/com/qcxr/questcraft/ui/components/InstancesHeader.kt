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
import com.qcxr.questcraft.ui.QuestLauncherScreen
import com.qcxr.questcraft.ui.theme.*

@Composable
fun InstancesHeader(
    onAddInstanceClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Button(
            onClick = onAddInstanceClick,
            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
            shape = RoundedCornerShape(2.dp),
            contentPadding = PaddingValues(horizontal = 16.dp, vertical = 10.dp),
            modifier = Modifier.height(40.dp)
        ) {
            Text(text = "+ " + stringResource(R.string.add_instance), color = Color.White, fontWeight = FontWeight.Black, fontSize = 12.sp)
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        // View Toggles
        Row(
            modifier = Modifier
                .height(40.dp)
                .background(SurfaceDark, RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp))
        ) {
            Box(modifier = Modifier.fillMaxHeight().aspectRatio(1f).padding(8.dp).background(AccentGreen.copy(alpha = 0.2f), RoundedCornerShape(2.dp)), contentAlignment = Alignment.Center) {
                Box(modifier = Modifier.size(14.dp).background(AccentGreen, RoundedCornerShape(1.dp)))
            }
            Box(modifier = Modifier.fillMaxHeight().aspectRatio(1f).padding(8.dp).background(Color.Transparent), contentAlignment = Alignment.Center) {
                Box(modifier = Modifier.size(14.dp).background(TextSecondary, RoundedCornerShape(1.dp)))
            }
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        // Search Bar
        Row(
            modifier = Modifier
                .width(400.dp)
                .height(40.dp)
                .background(SurfaceDark, RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp))
                .padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(modifier = Modifier.size(16.dp).background(TextSecondary)) // Search Icon
            Spacer(modifier = Modifier.width(12.dp))
            Text(text = stringResource(R.string.filter_instances), color = TextSecondary, fontSize = 14.sp)
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        // Filter Icon
        Box(
            modifier = Modifier
                .size(40.dp)
                .background(SurfaceDark, RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp)),
            contentAlignment = Alignment.Center
        ) {
            Box(modifier = Modifier.size(18.dp).background(TextSecondary))
        }
    }
}

@Preview(widthDp = 1000)
@Composable
fun InstancesHeaderPreview() {
    QuestCraftTheme {
        InstancesHeader(onAddInstanceClick = {})
    }
}