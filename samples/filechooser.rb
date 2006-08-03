require 'rjb'

Rjb::load

SwingUtilities = Rjb::import('javax.swing.SwingUtilities')
JThread = Rjb::import('java.lang.Thread')
JFileChooser = Rjb::import('javax.swing.JFileChooser')
class Run
  def initialize(&block)
    @block = block
  end
  def run
puts 'go-hello'
    @block.call
puts 'ret-hello'
  end
end

class FileChooser
  @@klass = JFileChooser
  def initialize(ext = '*', desc = 'any files')
    @selected = nil
  end

  def show()
    run = Rjb::bind(Run.new do
puts 'hello'
		      @selected = nil
		      chooser = @@klass.new()
puts 'hello'
		      ret = chooser.showOpenDialog(nil)
puts 'hello'
		      if ret == @@klass.APPROVE_OPTION 
			@selected = chooser.getSelectedFile
		      end
		    end, 'java.lang.Runnable')
    SwingUtilities.invokeAndWait(run)
  end
  attr_reader :selected
end

f = FileChooser.new
if f.show == 0
  puts f.selected.getAbsolutePath
end
puts 'bye'
