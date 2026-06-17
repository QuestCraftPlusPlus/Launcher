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
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
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
            .aspectRatio(1.4f)
            .background(SurfaceDark, RoundedCornerShape(8.dp))
            .border(
                width = if (isSelected) 2.dp else 1.dp,
                color = if (isSelected) AccentGreen else DividerColor,
                shape = RoundedCornerShape(8.dp)
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
                    .size(60.dp)
                    .background(instance.color.copy(alpha = 0.3f), RoundedCornerShape(4.dp))
                    .padding(8.dp),
                contentAlignment = Alignment.Center
            ) {
                // Lightning bolt placeholder
                Box(modifier = Modifier.size(30.dp).background(instance.color, RoundedCornerShape(2.dp)))
            }
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = instance.jLibInstance.instanceName, color = TextPrimary, fontWeight = FontWeight.Bold, fontSize = 16.sp)
            // TODO: Change versionType to loaderName
            Text(text = "${instance.jLibInstance.versionName} • ${instance.jLibInstance.versionType}", color = AccentGreen, fontSize = 14.sp)
        }
        
        // Menu dots
        Box(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .size(34.dp)
                .border(1.dp, DividerColor, RoundedCornerShape(4.dp)),
            contentAlignment = Alignment.Center
        ) {
            Text("...", color = TextSecondary, fontSize = 12.sp)
        }
    }
}