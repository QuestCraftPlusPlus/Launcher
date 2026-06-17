package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
fun TopBrandBar(modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(24.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = "QUESTCRAFT",
            color = AccentGreen,
            fontWeight = FontWeight.Black,
            fontSize = 24.sp
        )
        Spacer(modifier = Modifier.width(16.dp))
        Box(modifier = Modifier.size(1.dp, 24.dp).background(DividerColor))
        Spacer(modifier = Modifier.width(16.dp))
        Text(
            text = stringResource(R.string.instances).uppercase(),
            color = TextSecondary,
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold
        )
        Spacer(modifier = Modifier.weight(1f))

        // Status indicator
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .background(SurfaceDark, RoundedCornerShape(4.dp))
                .padding(horizontal = 12.dp, vertical = 6.dp)
                .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
        ) {
            Box(modifier = Modifier.size(12.dp).background(StatusStable, RoundedCornerShape(4.dp)))
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = "STABLE", color = TextPrimary, fontSize = 16.sp)
        }

        Spacer(modifier = Modifier.width(16.dp))

        // Notification Icon Placeholder
        Box(modifier = Modifier.size(40.dp).background(SurfaceDark, RoundedCornerShape(4.dp)).border(1.dp, DividerColor, RoundedCornerShape(4.dp)), contentAlignment = Alignment.Center) {
            Box(modifier = Modifier.size(22.dp).background(Color.Gray))
        }

        Spacer(modifier = Modifier.width(16.dp))

        // Profile Placeholder
        Box(modifier = Modifier.size(40.dp).background(AccentGreen, RoundedCornerShape(4.dp)))
    }
}

@Preview(widthDp = 1280)
@Composable
fun TopBrandBarPreview() {
    QuestCraftTheme {
        TopBrandBar()
    }
}