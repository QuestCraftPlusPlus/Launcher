package com.qcxr.questcraft.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.microsoft.aad.msal4j.DeviceCode
import com.qcxr.questcraft.MainActivity
import com.qcxr.questcraft.ui.components.AddInstanceOverlay
import com.qcxr.questcraft.ui.components.BottomControlBar
import com.qcxr.questcraft.ui.components.FooterBar
import com.qcxr.questcraft.ui.components.Instance
import com.qcxr.questcraft.ui.components.InstanceGrid
import com.qcxr.questcraft.ui.components.InstancesHeader
import com.qcxr.questcraft.ui.components.SideBar
import com.qcxr.questcraft.ui.components.TopBrandBar
import com.qcxr.questcraft.ui.theme.BackgroundDark
import com.qcxr.questcraft.ui.theme.QuestCraftTheme
import com.qcxr.questcraft.utils.Constants
import org.angelauramc.judgelib.JudgeLibAPI
import org.angelauramc.judgelib.impl.InitInfo

@Composable
fun QuestLauncherScreen() {
    val instances = remember {
        // TODO: Temp?
        val instancesList = mutableListOf<Instance>()
        val instances = MainActivity.judgeLibAPI.getInstances(Constants.INSTANCE_ROOT_PATH())
        for (instance in instances) {
            instancesList.add(Instance(instance, Color.Green))
        }
        instancesList
    }
    var selectedInstanceIndex by remember { mutableIntStateOf(0) }
    var selectedSideBarItem by remember { mutableStateOf("INSTANCES") }
    var showAddInstanceOverlay by remember { mutableStateOf(false) }

    Box(modifier = Modifier.fillMaxSize()) {
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
                        InstancesHeader(
                            onAddInstanceClick = { showAddInstanceOverlay = true }
                        )
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

        if (showAddInstanceOverlay) {
            AddInstanceOverlay(
                onDismiss = { showAddInstanceOverlay = false },
                onCreate = { name, version, loader ->

                    showAddInstanceOverlay = false
                }
            )
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