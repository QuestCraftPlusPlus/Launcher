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
            .width(120.dp)
            .fillMaxHeight()
            .background(SurfaceDark)
            .padding(vertical = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        SideBarItem(stringResource(R.string.quest), isSelected = selectedItem == stringResource(R.string.quest), onClick = { onItemClick(it) })
        Spacer(modifier = Modifier.height(24.dp))
        SideBarItem(stringResource(R.string.instances), isSelected = selectedItem == stringResource(R.string.instances), onClick = { onItemClick(it) })
        SideBarItem(stringResource(R.string.mods), isSelected = selectedItem == stringResource(R.string.mods), onClick = { onItemClick(it) })
        SideBarItem(stringResource(R.string.packs), isSelected = selectedItem == stringResource(R.string.packs), onClick = { onItemClick(it) })
        SideBarItem(stringResource(R.string.servers), isSelected = selectedItem == stringResource(R.string.servers), onClick = { onItemClick(it) })
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
            .padding(vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .size(40.dp)
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
                    .size(28.dp)
                    .background(if (isSelected) AccentGreen else Color.Gray, RoundedCornerShape(2.dp))
            )
        }
        Text(
            text = label,
            color = if (isSelected) AccentGreen else TextSecondary,
            fontSize = 14.sp,
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