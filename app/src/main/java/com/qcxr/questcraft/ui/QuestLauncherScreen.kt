package com.qcxr.questcraft.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.qcxr.questcraft.MainActivity
import com.qcxr.questcraft.ui.components.AddInstanceOverlay
import com.qcxr.questcraft.ui.components.BottomControlBar
import com.qcxr.questcraft.ui.components.FooterBar
import com.qcxr.questcraft.ui.components.InstallationOverlay
import com.qcxr.questcraft.ui.components.Instance
import com.qcxr.questcraft.ui.components.InstanceGrid
import com.qcxr.questcraft.ui.components.InstancesHeader
import com.qcxr.questcraft.ui.components.SideBar
import com.qcxr.questcraft.ui.components.TopBrandBar
import com.qcxr.questcraft.ui.theme.BackgroundDark
import com.qcxr.questcraft.ui.theme.QuestCraftTheme
import com.qcxr.questcraft.utils.Constants
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.angelauramc.judgelib.installer.LoaderType

@Composable
fun QuestLauncherScreen() {
    val scope = rememberCoroutineScope()
    val instances = remember {
        mutableStateListOf<Instance>().apply {
            val existingInstances = MainActivity.judgeLibAPI.getInstances(Constants.INSTANCE_ROOT_PATH())
            addAll(existingInstances.map { Instance(it, Color.Green) })
        }
    }
    var selectedInstanceIndex by remember { mutableIntStateOf(0) }
    var selectedSideBarItem by remember { mutableStateOf("INSTANCES") }
    var showAddInstanceOverlay by remember { mutableStateOf(false) }

    var showInstallingOverlay by remember { mutableStateOf(false) }
    var installProgress by remember { mutableFloatStateOf(0f) }
    var installStatus by remember { mutableStateOf("Downloading Assets...") }
    var installCurrentFile by remember { mutableStateOf("") }
    var installProgressDetail by remember { mutableStateOf("") }
    var isInstallationFinished by remember { mutableStateOf(false) }

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
                    showInstallingOverlay = true
                    isInstallationFinished = false
                    installProgress = 0f
                    installStatus = "Downloading Assets..."
                    installCurrentFile = "minecraft-$version-client.jar"
                    installProgressDetail = "0 / 124 MB"

                    scope.launch {
                        // Set up real progress callback from JudgeLibAPI
                        MainActivity.judgeLibAPI.installProgressCallback { progress, fileName ->
                            installProgress = progress.toFloat() / 100f
                            installCurrentFile = fileName
                            installProgressDetail = "" // Reset or update if more info is available
                        }

                        val versionObj = withContext(Dispatchers.IO) {
                            val mcVersion = LoaderType.VANILLA.metadata.getMinecraftVersion(version)
                            MainActivity.judgeLibAPI.installVersion(
                                mcVersion,
                                Constants.MINECRAFT_ASSETS_PATH(),
                                Constants.MINECRAFT_LIBRARIES_PATH()
                            )
                        }

                        withContext(Dispatchers.IO) {
                            MainActivity.judgeLibAPI.createInstance(
                                name,
                                versionObj,
                                Constants.INSTANCE_ROOT_PATH(),
                                Constants.MINECRAFT_ASSETS_PATH()
                            )
                        }

                        installProgress = 1f
                        isInstallationFinished = true
                        
                        // Refresh instances list
                        val updatedInstances = withContext(Dispatchers.IO) {
                            MainActivity.judgeLibAPI.getInstances(Constants.INSTANCE_ROOT_PATH())
                        }
                        instances.clear()
                        instances.addAll(updatedInstances.map { Instance(it, Color.Green) })
                    }
                }
            )
        }

        if (showInstallingOverlay) {
            InstallationOverlay(
                isFinished = isInstallationFinished,
                progress = installProgress,
                statusText = installStatus,
                currentFile = installCurrentFile,
                progressDetail = installProgressDetail,
                onDone = {
                    showInstallingOverlay = false
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