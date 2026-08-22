package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.R
import com.qcxr.questcraft.ui.theme.*

@Composable
fun AddInstanceOverlay(
    onDismiss: () -> Unit,
    onCreate: (name: String, version: String, loader: String) -> Unit
) {
    var instanceName by remember { mutableStateOf(TextFieldValue(text = "")) }
    var selectedVersion by remember { mutableStateOf("1.20.1") }
    var selectedLoader by remember { mutableStateOf("Fabric") }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.7f))
            .clickable(enabled = true, onClick = onDismiss),
        contentAlignment = Alignment.Center
    ) {
        Surface(
            modifier = Modifier
                .width(500.dp)
                .clickable(enabled = false, onClick = {}), // Prevent clicks from passing through
            color = SurfaceDark,
            shape = RoundedCornerShape(8.dp)
        ) {
            Column(
                modifier = Modifier
                    .border(1.dp, DividerColor, RoundedCornerShape(8.dp))
                    .padding(24.dp)
            ) {
                Text(
                    text = stringResource(R.string.create_instance),
                    color = TextPrimary,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
                
                Spacer(modifier = Modifier.height(24.dp))
                
                Text(text = stringResource(R.string.instance_name), color = TextSecondary, fontSize = 12.sp)
                Spacer(modifier = Modifier.height(8.dp))
                XrOutlinedTextField(
                    value = instanceName,
                    onValueChange = { instanceName = it },
                    modifier = Modifier.fillMaxWidth(),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = AccentGreen,
                        unfocusedBorderColor = DividerColor,
                        focusedTextColor = TextPrimary,
                        unfocusedTextColor = TextPrimary
                    ),
                    singleLine = true
                )
                
                Spacer(modifier = Modifier.height(16.dp))
                
                Row(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(text = stringResource(R.string.select_version), color = TextSecondary, fontSize = 12.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(56.dp)
                                .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                                .padding(horizontal = 16.dp),
                            contentAlignment = Alignment.CenterStart
                        ) {
                            Text(text = selectedVersion, color = TextPrimary)
                        }
                    }
                    
                    Spacer(modifier = Modifier.width(16.dp))
                    
                    Column(modifier = Modifier.weight(1f)) {
                        Text(text = stringResource(R.string.select_loader), color = TextSecondary, fontSize = 12.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(56.dp)
                                .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                                .padding(horizontal = 16.dp),
                            contentAlignment = Alignment.CenterStart
                        ) {
                            Text(text = selectedLoader, color = TextPrimary)
                        }
                    }
                }
                
                Spacer(modifier = Modifier.height(32.dp))
                
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    Button(
                        onClick = onDismiss,
                        colors = ButtonDefaults.buttonColors(containerColor = Color.Transparent),
                        shape = RoundedCornerShape(4.dp)
                    ) {
                        Text(text = stringResource(R.string.cancel), color = TextSecondary)
                    }
                    
                    Spacer(modifier = Modifier.width(8.dp))
                    
                    Button(
                        onClick = { onCreate(instanceName.text, selectedVersion, selectedLoader) },
                        colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
                        shape = RoundedCornerShape(4.dp),
                        enabled = instanceName.text.isNotBlank()
                    ) {
                        Text(text = stringResource(R.string.create), color = Color.White)
                    }
                }
            }
        }
    }
}

@Preview(widthDp = 1280, heightDp = 720)
@Composable
fun AddInstanceOverlayPreview() {
    QuestCraftTheme {
        AddInstanceOverlay(onDismiss = {}, onCreate = { _, _, _ -> })
    }
}
