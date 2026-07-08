package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.VideogameAsset
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.ui.theme.*

@Composable
fun InstallationOverlay(
    isFinished: Boolean,
    progress: Float,
    statusText: String,
    currentFile: String,
    progressDetail: String,
    onDone: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.7f))
            .clickable(enabled = true, onClick = { /* Prevent clicks from closing if we want */ }),
        contentAlignment = Alignment.Center
    ) {
        Surface(
            modifier = Modifier
                .width(600.dp)
                .clickable(enabled = false, onClick = {}),
            color = SurfaceDark,
            shape = RoundedCornerShape(8.dp)
        ) {
            Column(
                modifier = Modifier
                    .border(1.dp, DividerColor, RoundedCornerShape(8.dp))
            ) {
                // Header
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(24.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(40.dp)
                            .background(AccentGreen, RoundedCornerShape(4.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.VideogameAsset,
                            contentDescription = null,
                            modifier = Modifier.size(24.dp),
                            tint = Color.White
                        )
                    }
                    Spacer(modifier = Modifier.width(16.dp))
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = "CREATE INSTANCE",
                            color = TextPrimary,
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = if (isFinished) "SUCCESS" else "STEP 3: INSTALLING",
                            color = TextSecondary,
                            fontSize = 12.sp
                        )
                    }
                    if (isFinished) {
                        IconButton(onClick = onDone) {
                            Icon(
                                imageVector = Icons.Default.Close,
                                contentDescription = "Close",
                                tint = TextSecondary,
                                modifier = Modifier.size(20.dp)
                            )
                        }
                    }
                }

                HorizontalDivider(color = DividerColor, thickness = 1.dp)

                // Content
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(48.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    if (!isFinished) {
                        // Installing State
                        Box(
                            modifier = Modifier
                                .size(80.dp)
                                .border(1.dp, DividerColor, CircleShape),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Download,
                                contentDescription = null,
                                modifier = Modifier.size(32.dp),
                                tint = AccentGreen
                            )
                        }

                        Spacer(modifier = Modifier.height(48.dp))

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.Bottom
                        ) {
                            Text(
                                text = statusText.uppercase(),
                                color = TextPrimary,
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold
                            )
                            Text(
                                text = "${(progress * 100).toInt()}%",
                                color = AccentGreen,
                                fontSize = 14.sp,
                                fontWeight = FontWeight.Bold
                            )
                        }

                        Spacer(modifier = Modifier.height(16.dp))

                        LinearProgressIndicator(
                            progress = { progress },
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(12.dp),
                            color = AccentGreen,
                            trackColor = DividerColor,
                            strokeCap = androidx.compose.ui.graphics.StrokeCap.Round
                        )

                        Spacer(modifier = Modifier.height(16.dp))

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text(
                                text = currentFile,
                                color = TextSecondary,
                                fontSize = 12.sp
                            )
                            Text(
                                text = progressDetail,
                                color = TextSecondary,
                                fontSize = 12.sp
                            )
                        }
                    } else {
                        // Finished State
                        Box(
                            modifier = Modifier.size(100.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(80.dp)
                                    .background(AccentGreen, RoundedCornerShape(4.dp)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.VideogameAsset,
                                    contentDescription = null,
                                    modifier = Modifier.size(48.dp),
                                    tint = Color.White
                                )
                            }
                            Box(
                                modifier = Modifier
                                    .align(Alignment.BottomEnd)
                                    .offset(x = (-4).dp, y = (-4).dp)
                                    .size(24.dp)
                                    .background(SurfaceDark, CircleShape)
                                    .padding(2.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.CheckCircle,
                                    contentDescription = null,
                                    tint = AccentGreen,
                                    modifier = Modifier.fillMaxSize()
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(24.dp))

                        Text(
                            text = "INSTANCE CREATED",
                            color = TextPrimary,
                            fontSize = 24.sp,
                            fontWeight = FontWeight.Bold
                        )

                        Spacer(modifier = Modifier.height(8.dp))

                        Text(
                            text = "Your new instance is ready to play!",
                            color = TextSecondary,
                            fontSize = 16.sp
                        )

                        Spacer(modifier = Modifier.height(48.dp))

                        HorizontalDivider(color = DividerColor, thickness = 1.dp)

                        Spacer(modifier = Modifier.height(24.dp))

                        Button(
                            onClick = onDone,
                            modifier = Modifier.fillMaxWidth().height(48.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
                            shape = RoundedCornerShape(4.dp)
                        ) {
                            Text(text = "DONE", color = Color.White, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }
    }
}

@Preview(widthDp = 1280, heightDp = 720)
@Composable
fun InstallationOverlayInstallingPreview() {
    QuestCraftTheme {
        InstallationOverlay(
            isFinished = false,
            progress = 0.6f,
            statusText = "Downloading Assets...",
            currentFile = "minecraft-1.20.4-client.jar",
            progressDetail = "124 / 124 MB",
            onDone = {}
        )
    }
}

@Preview(widthDp = 1280, heightDp = 720)
@Composable
fun InstallationOverlayFinishedPreview() {
    QuestCraftTheme {
        InstallationOverlay(
            isFinished = true,
            progress = 1f,
            statusText = "",
            currentFile = "",
            progressDetail = "",
            onDone = {}
        )
    }
}
