package com.qcxr.questcraft.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowForward
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.qcxr.questcraft.MainActivity
import com.qcxr.questcraft.ui.theme.*
import org.angelauramc.judgelib.util.json.auth.JudgeLibAccount
import java.util.concurrent.CompletableFuture
import kotlin.time.Duration.Companion.seconds

enum class LoginStep {
    CONFIRM,
    GENERATING,
    AUTHENTICATE,
    CONNECTING,
    FINISH
}

var accountFuture = CompletableFuture<JudgeLibAccount>()
var accountResult = mutableStateOf<JudgeLibAccount?>(null)

@Composable
fun MicrosoftLoginOverlay(
    onDismiss: () -> Unit,
    onFinished: () -> Unit
) {
    var currentStep by remember { mutableStateOf(LoginStep.CONFIRM) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.8f))
            .clickable(enabled = false) {},
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .width(480.dp)
                .background(CardBackground, RoundedCornerShape(4.dp))
                .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
        ) {
            // Header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box(
                    modifier = Modifier
                        .size(32.dp)
                        .background(AccentBlue, RoundedCornerShape(2.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Language,
                        contentDescription = null,
                        tint = Color.White,
                        modifier = Modifier.size(20.dp)
                    )
                }
                Spacer(modifier = Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = "MICROSOFT LOGIN",
                        color = TextPrimary,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Black
                    )
                    Text(
                        text = when (currentStep) {
                            LoginStep.CONFIRM -> "STEP 1: CONFIRM"
                            LoginStep.GENERATING -> "STEP 2: GENERATING"
                            LoginStep.AUTHENTICATE -> "STEP 3: AUTHENTICATE"
                            LoginStep.CONNECTING -> "CONNECTING..."
                            LoginStep.FINISH -> "FINISH"
                        },
                        color = TextSecondary,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
                Box(
                    modifier = Modifier
                        .size(32.dp)
                        .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                        .clickable { onDismiss() },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Close",
                        tint = TextSecondary,
                        modifier = Modifier.size(16.dp)
                    )
                }
            }

            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(DividerColor)
            )

            // Content
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(48.dp),
                contentAlignment = Alignment.Center
            ) {
                when (currentStep) {
                    LoginStep.CONFIRM -> ConfirmStep(onContinue = { currentStep = LoginStep.GENERATING })
                    LoginStep.GENERATING -> GeneratingStep(onNext = { currentStep = LoginStep.AUTHENTICATE })
                    LoginStep.AUTHENTICATE -> AuthenticateStep(onNext = { currentStep = LoginStep.CONNECTING })
                    LoginStep.CONNECTING -> ConnectingStep(onNext = { currentStep = LoginStep.FINISH })
                    LoginStep.FINISH -> FinishStep(onFinish = onFinished)
                }
            }
        }
    }
}

@Composable
fun GeneratingStep(onNext: () -> Unit) {
    LaunchedEffect(Unit) {
        accountFuture = MainActivity.judgeLibAPI.startLogin(null)
        while (MainActivity.userLoginCode == null) {
            kotlinx.coroutines.delay(1.seconds)
        }
        onNext()
    }
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        CircularProgressIndicator(color = AccentBlue, modifier = Modifier.size(48.dp))
        Spacer(modifier = Modifier.height(24.dp))
        Text(
            text = "GENERATING CODE...",
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Black
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "Waiting for Microsoft to generate a login code",
            color = TextSecondary,
            fontSize = 11.sp
        )
    }
}

@Composable
fun ConfirmStep(onContinue: () -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            modifier = Modifier
                .size(64.dp)
                .border(1.dp, DividerColor, RoundedCornerShape(32.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Default.Person,
                contentDescription = null,
                tint = TextSecondary,
                modifier = Modifier.size(32.dp)
            )
        }
        Spacer(modifier = Modifier.height(24.dp))
        Text(
            text = "ADD A NEW ACCOUNT",
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Black
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "You will be redirected to Microsoft to sign in.\nOnce authenticated, your Minecraft profile will\nbe added to the launcher.",
            color = TextSecondary,
            fontSize = 11.sp,
            textAlign = TextAlign.Center,
            lineHeight = 16.sp
        )
        Spacer(modifier = Modifier.height(32.dp))
        Button(
            onClick = onContinue,
            colors = ButtonDefaults.buttonColors(containerColor = AccentBlue),
            shape = RoundedCornerShape(4.dp),
            modifier = Modifier.fillMaxWidth().height(48.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("CONTINUE", color = Color.White, fontWeight = FontWeight.Black)
                Spacer(modifier = Modifier.width(8.dp))
                Icon(Icons.Default.ArrowForward, null, modifier = Modifier.size(16.dp))
            }
        }
    }
}

@Composable
fun AuthenticateStep(onNext: () -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = "Go to microsoft.com/link on any device\nand enter the code below:",
            color = TextSecondary,
            fontSize = 11.sp,
            textAlign = TextAlign.Center,
            lineHeight = 16.sp
        )
        Spacer(modifier = Modifier.height(24.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                modifier = Modifier
                    .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                    .padding(horizontal = 24.dp, vertical = 16.dp)
                    .clickable { onNext() } // Shortcut for demo
            ) {
                Text(
                    text = MainActivity.userLoginCode!!,
                    color = TextPrimary,
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Black,
                    letterSpacing = 2.sp
                )
            }
            Spacer(modifier = Modifier.width(8.dp))
            Box(
                modifier = Modifier
                    .size(56.dp)
                    .border(1.dp, DividerColor, RoundedCornerShape(4.dp))
                    .clickable { /* Copy */ },
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(Icons.Default.ContentCopy, null, tint = TextSecondary, modifier = Modifier.size(16.dp))
                    Text("COPY", color = TextSecondary, fontSize = 8.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
        Spacer(modifier = Modifier.height(24.dp))
        Text(
            text = "I HAVE ENTERED THE CODE",
            color = TextSecondary,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.clickable { onNext() }
        )
    }
}

@Composable
fun ConnectingStep(onNext: () -> Unit) {
    LaunchedEffect(Unit) {
        MainActivity.userLoginCode = null
        accountResult = mutableStateOf(accountFuture.get())
        onNext()
    }
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        CircularProgressIndicator(color = AccentBlue, modifier = Modifier.size(48.dp))
        Spacer(modifier = Modifier.height(24.dp))
        Text(
            text = "AUTHENTICATING...",
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Black
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "Waiting for Microsoft to approve the request",
            color = TextSecondary,
            fontSize = 11.sp
        )
    }
}

@Composable
fun FinishStep(onFinish: () -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            modifier = Modifier
                .size(64.dp)
                .background(AccentGreen, RoundedCornerShape(4.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(Icons.Default.Person, null, tint = Color.White, modifier = Modifier.size(32.dp))
        }
        Spacer(modifier = Modifier.height(24.dp))
        Text(
            text = "ACCOUNT ADDED",
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Black
        )
        Spacer(modifier = Modifier.height(8.dp))
        Row {
            Text("Welcome, ", color = AccentGreen, fontSize = 11.sp)
            Text(accountResult.value!!.username, color = AccentGreen, fontSize = 11.sp, fontWeight = FontWeight.Bold)
        }
        Spacer(modifier = Modifier.height(32.dp))
        Button(
            onClick = onFinish,
            colors = ButtonDefaults.buttonColors(containerColor = AccentGreen),
            shape = RoundedCornerShape(4.dp),
            modifier = Modifier.fillMaxWidth().height(48.dp)
        ) {
            Text("LET'S PLAY", color = Color.White, fontWeight = FontWeight.Black)
        }
    }
}

@Preview
@Composable
fun MicrosoftLoginOverlayPreview() {
    QuestCraftTheme {
        MicrosoftLoginOverlay(onDismiss = {}, onFinished = {})
    }
}
