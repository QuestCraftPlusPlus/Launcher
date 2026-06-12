package com.qcxr.questcraft.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.qcxr.questcraft.ui.components.*
import com.qcxr.questcraft.ui.theme.*

@Composable
fun QuestLauncherScreen() {
    val instances = remember {
        mutableStateListOf(
            Instance("MAIN SURVIVAL", "1.20.4", "Fabric", AccentGreen),
            Instance("SKYBLOCK ADVENTURE", "1.19.2", "Forge", InstanceIconBrown),
            Instance("CREATIVE SANDBOX", "1.21", "Fabric", InstanceIconGrey),
            Instance("HARDCORE VR", "1.20.1", "Quilt", InstanceIconGreen),
            Instance("VANILLA 1.8.9", "1.8.9", "Vanilla", InstanceIconGrey),
            Instance("RLCRAFT VR", "1.12.2", "Forge", InstanceIconRed),
        )
    }
    var selectedInstanceIndex by remember { mutableIntStateOf(0) }
    var selectedSideBarItem by remember { mutableStateOf("INSTANCES") }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = BackgroundDark
    ) {
        Row(modifier = Modifier.fillMaxSize()) {
            SideBar(
                selectedItem = selectedSideBarItem,
                onItemClick = { selectedSideBarItem = it }
            )
            Column(modifier = Modifier.fillMaxSize()) {
                TopBrandBar()
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .padding(horizontal = 24.dp)
                ) {
                    InstancesHeader()
                    InstanceGrid(
                        instances = instances,
                        selectedIndex = selectedInstanceIndex,
                        onInstanceClick = { selectedInstanceIndex = it },
                        modifier = Modifier.weight(1f)
                    )
                    BottomControlBar(
                        selectedInstance = instances.getOrNull(selectedInstanceIndex)
                    )
                }
                FooterBar()
            }
        }
    }
}

@Preview(showBackground = true, widthDp = 1280, heightDp = 720)
@Composable
fun QuestLauncherPreview() {
    QuestCraftTheme {
        QuestLauncherScreen()
    }
}