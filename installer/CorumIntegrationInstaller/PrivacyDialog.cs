namespace CorumIntegrationInstaller;

internal sealed class PrivacyDialog : Form
{
    public PrivacyDialog()
    {
        Text = "Privacy / Record verification information";
        StartPosition = FormStartPosition.CenterParent;
        MinimumSize = new Size(620, 460);
        Size = new Size(760, 620);
        ShowIcon = false;
        MaximizeBox = true;
        MinimizeBox = false;
        Font = new Font("Segoe UI", 9F);

        var textBox = new TextBox
        {
            Dock = DockStyle.Fill,
            Multiline = true,
            ReadOnly = true,
            ScrollBars = ScrollBars.Both,
            WordWrap = true,
            BackColor = SystemColors.Window,
            Text = PrivacyContent.Load()
        };

        var closeButton = new Button
        {
            Text = "Close",
            AutoSize = true,
            DialogResult = DialogResult.OK,
            Padding = new Padding(18, 4, 18, 4),
            Anchor = AnchorStyles.Right
        };

        var buttonPanel = new FlowLayoutPanel
        {
            Dock = DockStyle.Bottom,
            Height = 52,
            FlowDirection = FlowDirection.RightToLeft,
            Padding = new Padding(10)
        };
        buttonPanel.Controls.Add(closeButton);

        Controls.Add(textBox);
        Controls.Add(buttonPanel);
        AcceptButton = closeButton;
        CancelButton = closeButton;
    }
}
