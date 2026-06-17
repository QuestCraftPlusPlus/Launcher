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
            .padding(vertical = 16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Button(
            onClick = onAddInstanceClick,
            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
            shape = RoundedCornerShape(4.dp),
            contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp)
        ) {
            Text(text = "+ " + stringResource(R.string.add_instance), color = Color.White, fontWeight = FontWeight.Bold)
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        // View Toggles
        Row(modifier = Modifier.background(SurfaceDark, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp))) {
            Box(modifier = Modifier.padding(8.dp).size(20.dp).background(AccentGreen, RoundedCornerShape(2.dp)))
            Box(modifier = Modifier.padding(8.dp).size(20.dp).background(Color.Transparent))
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        // Search Bar
        Row(
            modifier = Modifier
                .width(300.dp)
                .background(SurfaceDark, RoundedCornerShape(4.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(modifier = Modifier.size(16.dp).background(Color.Gray)) // Search Icon
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = stringResource(R.string.filter_instances), color = TextSecondary, fontSize = 14.sp)
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        // Filter Icon
        Box(modifier = Modifier.size(36.dp).background(SurfaceDark, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
            Box(modifier = Modifier.size(18.dp).background(Color.Gray))
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