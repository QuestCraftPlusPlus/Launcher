package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.ui.theme.*
import org.angelauramc.judgelib.instance.JudgeLibInstance

data class Instance(val jLibInstance: JudgeLibInstance, val color: Color)

@Composable
fun InstanceGrid(
    instances: List<Instance>,
    selectedIndex: Int,
    onInstanceClick: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    LazyVerticalGrid(
        columns = GridCells.Fixed(4),
        horizontalArrangement = Arrangement.spacedBy(24.dp),
        verticalArrangement = Arrangement.spacedBy(24.dp),
        modifier = modifier
    ) {
        itemsIndexed(instances) { index, instance ->
            InstanceCard(
                instance = instance,
                isSelected = index == selectedIndex,
                onClick = { onInstanceClick(index) },
            )
        }
    }
}

@Composable
fun InstanceCard(instance: Instance, isSelected: Boolean, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1.25f)
            .background(SurfaceDark, RoundedCornerShape(2.dp))
            .border(
                width = if (isSelected) 1.dp else 0.dp,
                color = if (isSelected) AccentGreen else Color.Transparent,
                shape = RoundedCornerShape(4.dp)
            )
            .clickable { onClick() }
            .padding(16.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Box(
                modifier = Modifier
                    // box for icon
                    .size(70.dp)
                    .background(instance.color.copy(alpha = 0.2f), RoundedCornerShape(2.dp))
                    .border(1.dp, instance.color.copy(alpha = 0.5f), RoundedCornerShape(2.dp))
                    .padding(8.dp),
                contentAlignment = Alignment.Center
            ) {
                // Icon itself
                Box(modifier = Modifier.size(60.dp).background(instance.color, RoundedCornerShape(1.dp)))
            }
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = instance.jLibInstance.instanceName.uppercase(),
                color = TextPrimary,
                fontWeight = FontWeight.Black,
                fontSize = 18.sp
            )
            Spacer(modifier = Modifier.height(4.dp))
            Row {
                Text(text = instance.jLibInstance.versionName, color = AccentGreen, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                Text(text = " • ", color = TextSecondary, fontSize = 10.sp)
                Text(text = instance.jLibInstance.versionType, color = TextSecondary, fontSize = 12.sp)
            }
        }
        
        // Menu dots
        Box(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .size(24.dp)
                .background(Color.Black.copy(alpha = 0.2f), RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp)),
            contentAlignment = Alignment.Center
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Box(modifier = Modifier.size(2.dp).background(TextSecondary))
                Spacer(modifier = Modifier.height(2.dp))
                Box(modifier = Modifier.size(2.dp).background(TextSecondary))
                Spacer(modifier = Modifier.height(2.dp))
                Box(modifier = Modifier.size(2.dp).background(TextSecondary))
            }
        }
    }
}

@Preview(widthDp = 1280)
@Composable
fun InstanceGridPreview() {
    QuestCraftTheme {
        InstanceGrid(
            instances = emptyList(),
            selectedIndex = 0,
            onInstanceClick = {}
        )
    }
}