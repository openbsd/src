import java.awt.event.*;
import java.awt.*;
public class Closer extends WindowAdapter {

    public void windowClosing(WindowEvent e) {
        Window w = e.getWindow();
        w.dispose();
    }
}
