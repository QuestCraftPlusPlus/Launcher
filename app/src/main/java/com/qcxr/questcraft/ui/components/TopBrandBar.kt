package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.material.icons.Icons
import androidx.compose.foundation.Image
import androidx.compose.ui.layout.ContentScale
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
            .padding(top = 10.dp, bottom = 10.dp, start = 24.dp, end = 24.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Image(
            painter = painterResource(id = R.drawable.qctitlecomp),
            contentDescription = "QuestCraft",
            contentScale = ContentScale.FillBounds,
            modifier = Modifier
                .size(width = 300.dp, height = 50.dp)
                .aspectRatio(3f / 0.5f)

        )
        Spacer(modifier = Modifier.weight(1f))

        // Status indicator
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .height(40.dp)
                .background(SurfaceDark, RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp))
                .padding(horizontal = 12.dp)
        ) {
            Box(modifier = Modifier.size(18.dp).background(color=StatusStable, RoundedCornerShape(1.dp)))
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = "EXPERIMENTAL", color = TextSecondary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
            // color and text should change depending on what build it is
            // my guess and what colors should be are the following:

            // experimental (default unless changed for a release) = keep green ig
            // alpha/patreon = orange icon, ALPHA # where # is build # for vers
            // betas = yellow or pink, BETA #
        }   // releases = green, can be called stable or just state their version # in the box

        Spacer(modifier = Modifier.width(16.dp))

        // Account Name
        Box(
            modifier = Modifier
                .size(height = 40.dp, width = 220.dp)
                .background(SurfaceDark, RoundedCornerShape(2.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(2.dp))
                .padding(end = 10.dp),
            contentAlignment = Alignment.CenterEnd
        ) {
            Text(text = "JoeMinecraft", color = TextPrimary, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        }

        Spacer(modifier = Modifier.width(16.dp))

        // Profile Placeholder
        Box(
            modifier = Modifier
                .size(40.dp)
                .background(AccentGreen, RoundedCornerShape(2.dp))
        )
    }
}

@Preview(widthDp = 1280)
@Composable
fun TopBrandBarPreview() {
    QuestCraftTheme {
        TopBrandBar()
    }
}