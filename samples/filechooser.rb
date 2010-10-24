#!/usr/local/bin/ruby -Ks
#coding: cp932

require 'rjb'

Rjb::load

unless RUBY_VERSION =~ /^1\.9/
  class String
    def encode(s)
      self
    end
  end
end

class FileChooser
  @@klass = Rjb::import('javax.swing.JFileChooser')
  def initialize(ext = '*', desc = 'any files')
    @selected = nil
  end

  def show()
    chooser = @@klass.new
    if chooser.showOpenDialog(nil) == @@klass.APPROVE_OPTION 
      @selected = chooser.getSelectedFile
    end
  end
  attr_reader :selected
end

f = FileChooser.new
if f.show  
  puts f.selected.getAbsolutePath.encode('cp932')
end
puts 'bye'
