package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
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
fun SideBar(
    selectedItem: String,
    onItemClick: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .width(80.dp)
            .fillMaxHeight()
            .background(SurfaceDark)
            .padding(vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Logo / Home
        Box(
            modifier = Modifier
                .padding(bottom = 16.dp)
                .size(56.dp)
                .background(AccentGreen.copy(alpha = 0.2f), RoundedCornerShape(4.dp))
                .padding(8.dp),
            contentAlignment = Alignment.Center
        ) {
            Box(modifier = Modifier.size(32.dp).background(AccentGreen, RoundedCornerShape(2.dp)))
        }
        
        Spacer(modifier = Modifier.height(8.dp))
        
        SideBarItem(stringResource(R.string.instances), isSelected = selectedItem == stringResource(R.string.instances), onClick = { onItemClick(it) })
        SideBarItem(stringResource(R.string.accounts), isSelected = selectedItem == stringResource(R.string.accounts), onClick = { onItemClick(it) })
        SideBarItem(stringResource(R.string.settings), isSelected = selectedItem == stringResource(R.string.settings), onClick = { onItemClick(it) })
        
        Spacer(modifier = Modifier.weight(1f))
        
        SideBarItem(stringResource(R.string.quit), isSelected = selectedItem == stringResource(R.string.quit), onClick = { onItemClick(it) })
    }
}

@Composable
fun SideBarItem(label: String, isSelected: Boolean = false, onClick: (String) -> Unit = {}) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick(label) }
            .padding(vertical = 12.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .size(44.dp)
                .border(
                    width = if (isSelected) 2.dp else 0.dp,
                    color = if (isSelected) AccentGreen else Color.Transparent,
                    shape = RoundedCornerShape(4.dp)
                )
                .padding(4.dp),
            contentAlignment = Alignment.Center
        ) {
            // Icon Placeholder
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .background(if (isSelected) AccentGreen else TextSecondary, RoundedCornerShape(2.dp))
            )
        }
        Text(
            text = label,
            color = if (isSelected) AccentGreen else TextSecondary,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(top = 4.dp)
        )
    }
}

@Preview(heightDp = 720)
@Composable
fun SideBarPreview() {
    QuestCraftTheme {
        SideBar(selectedItem = "INSTANCES", onItemClick = {})
    }
}