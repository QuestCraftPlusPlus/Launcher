package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Icon
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.MainActivity
import com.qcxr.questcraft.R
import com.qcxr.questcraft.ui.theme.*
import com.qcxr.questcraft.utils.Constants
import org.angelauramc.judgelib.util.json.auth.JudgeLibAccount
import java.util.Collections.addAll

data class AccountProfile(
    val name: String,
    val uuid: String,
    val isActive: Boolean = false,
    val expireTime: String,
    val avatarColor: Color = Color.Gray,
    val originalAccount: JudgeLibAccount? = null
)

@Composable
fun AccountsScreen(
    selectedAccount: JudgeLibAccount?,
    onAccountSelected: (JudgeLibAccount) -> Unit,
    onAddAccountClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val profiles = remember { mutableStateListOf<AccountProfile>() }

    LaunchedEffect(Unit) {
        try {
            val existingAccounts = MainActivity.judgeLibAPI.allAccounts()
            profiles.clear()
            profiles.addAll(existingAccounts.mapIndexed { index, it ->
                AccountProfile(
                    it.username,
                    it.uuid,
                    false,
                    it.expiresAt.toString(),
                    if (index % 2 == 0) AccentGreen else AccentBlue,
                    it
                )
            })
        } catch (e: Exception) {
            android.util.Log.e("AccountsScreen", "Failed to load accounts. JudgeLibAPI might not be initialized.", e)
        }
    }

    Column(modifier = modifier.fillMaxSize()) {
        Text(
            text = "PROFILES",
            color = TextPrimary,
            fontSize = 24.sp,
            fontWeight = FontWeight.Black
        )
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = "${profiles.size} ACCOUNTS",
                color = TextSecondary,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold
            )
            Spacer(modifier = Modifier.width(16.dp))
            Text(
                text = "TAP TO SWITCH",
                color = TextSecondary,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold
            )
        }
        
        Spacer(modifier = Modifier.height(24.dp))
        
        LazyColumn(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            contentPadding = PaddingValues(bottom = 24.dp)
        ) {
            items(profiles) { profile ->
                AccountItem(
                    profile = profile.copy(isActive = profile.uuid == selectedAccount?.uuid),
                    onSelect = { 
                        profile.originalAccount?.let { onAccountSelected(it) }
                    }
                )
            }
            item {
                AddAccountButton(onClick = onAddAccountClick)
            }
        }
    }
}

@Composable
fun AccountItem(profile: AccountProfile, onSelect: () -> Unit) {
    val borderColor = if (profile.isActive) AccentGreen else DividerColor
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(CardBackground, RoundedCornerShape(4.dp))
            .border(1.dp, borderColor, RoundedCornerShape(4.dp))
            .clickable { onSelect() }
            .padding(16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Avatar
        Box(
            modifier = Modifier
                .size(64.dp)
                .background(SurfaceDark, RoundedCornerShape(4.dp))
                .border(1.dp, borderColor, RoundedCornerShape(4.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Default.Person,
                contentDescription = null,
                tint = TextSecondary,
                modifier = Modifier.size(32.dp)
            )
            
            if (profile.isActive) {
                Box(
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .offset(x = 4.dp, y = (-4).dp)
                        .size(20.dp)
                        .background(AccentGreen, RoundedCornerShape(4.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Check,
                        contentDescription = null,
                        tint = Color.White,
                        modifier = Modifier.size(14.dp)
                    )
                }
            }
        }
        
        Spacer(modifier = Modifier.width(20.dp))
        
        Column(modifier = Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = profile.name,
                    color = TextPrimary,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Black
                )
                if (profile.isActive) {
                    Spacer(modifier = Modifier.width(8.dp))
                    Box(
                        modifier = Modifier
                            .background(AccentGreen.copy(alpha = 0.2f), RoundedCornerShape(2.dp))
                            .padding(horizontal = 4.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "ACTIVE",
                            color = AccentGreen,
                            fontSize = 8.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            Row {
                Column {
                    Text("EXPIRES", color = TextSecondary, fontSize = 8.sp, fontWeight = FontWeight.Bold)
                    Text(profile.expireTime, color = TextPrimary, fontSize = 12.sp)
                }
                Spacer(modifier = Modifier.width(24.dp))
            }
        }
        
        if (!profile.isActive) {
            Button(
                onClick = onSelect,
                colors = ButtonDefaults.buttonColors(containerColor = if (profile.name == "ALEXBUILDER") AccentBlue else AccentGreen),
                shape = RoundedCornerShape(4.dp),
                modifier = Modifier.height(36.dp),
                contentPadding = PaddingValues(horizontal = 16.dp)
            ) {
                Text("SELECT", color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Black)
            }
            Spacer(modifier = Modifier.width(8.dp))
        }

        Column {
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .background(SurfaceDark, RoundedCornerShape(4.dp))
                    .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                    .clickable { /* TODO */ },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Default.Edit,
                    contentDescription = null,
                    tint = TextSecondary,
                    modifier = Modifier.size(16.dp)
                )
            }
            if (!profile.isActive) {
                Spacer(modifier = Modifier.height(8.dp))
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .background(SurfaceDark, RoundedCornerShape(4.dp))
                        .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                        .clickable { /* TODO */ },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Delete,
                        contentDescription = null,
                        tint = TextSecondary,
                        modifier = Modifier.size(16.dp)
                    )
                }
            }
        }
    }
}

@Composable
fun AddAccountButton(onClick: () -> Unit) {
    val stroke = Stroke(width = 2f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(10f, 10f), 0f))
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(64.dp)
            .clickable { onClick() },
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            drawRoundRect(color = DividerColor, style = stroke)
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = Icons.Default.Add,
                contentDescription = null,
                tint = TextSecondary,
                modifier = Modifier.size(16.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text("ADD MICROSOFT ACCOUNT", color = TextSecondary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Preview(widthDp = 1000)
@Composable
fun AccountsScreenPreview() {
    QuestCraftTheme {
        Box(modifier = Modifier.background(BackgroundDark).padding(24.dp)) {
            AccountsScreen(
                selectedAccount = null,
                onAccountSelected = {},
                onAddAccountClick = {}
            )
        }
    }
}
