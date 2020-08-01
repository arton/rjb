lib_path = File.expand_path(File.join(File.dirname(__FILE__), 'lib'))
$LOAD_PATH.unshift(lib_path) unless $LOAD_PATH.include?(lib_path)

require 'rake'
require 'rjb/version'

RJB_GEMSPEC = Gem::Specification.new do |s|
  s.author = 'arton'
  s.email = 'artonx@gmail.com'
  s.name = 'rjb'
  s.description = 'RJB is a Bridge library which connects Ruby and Java'\
                  ' code using the Java Native Interface.'
  s.summary = 'Ruby Java Bridge'
  s.homepage = 'http://www.artonx.org/collabo/backyard/?RubyJavaBridge'
  s.version = Rjb::VERSION
  s.require_path = 'lib'
  # @todo We need a meaningful explanation for the end user.
  s.requirements << 'JDK 5.0'
  s.license = 'LGPL-2.1-or-later'
  # @todo Do we need to support these old versions?
  s.required_ruby_version = '>= 1.8.2'
  # @todo Do we really need all the source code?
  s.files = FileList['ext/*.java', 'ext/*.c', 'ext/*.h', 'ext/depend.erb',
                     'data/rjb/**/*.class', 'lib/*.rb', 'lib/rjb/*.rb',
                     'samples/**/*.rb', 'test/*.rb', 'test/**/*.class',
                     'test/*.jar', 'COPYING', 'ChangeLog', 'readme.*',
                     'README.md']

  # @todo We need some restrictions for JRuby and other plattforms
  #   following RbConfig::CONFIG['host_os'] definitions.
  #   A possible better solution is `rake-compiler`.
  case RUBY_PLATFORM
  when /mswin/
    s.platform = Gem::Platform::CURRENT
    FileUtils.cp 'ext/rjbcore.so', 'lib/rjbcore.so'
    s.files << 'lib/rjbcore.so'
    # @todo We need a meaningful explanation for the end user.
    s.requirements << 'VC6 version of Ruby'
  when /mingw/
    s.platform = Gem::Platform::CURRENT
    FileUtils.cp 'ext/rjbcore.so', 'lib/rjbcore.so'
    s.files << 'lib/rjbcore.so'
  when /darwin/
    s.platform = Gem::Platform::CURRENT
    FileUtils.cp 'ext/rjbcore.bundle', 'lib/rjbcore.bundle'
    s.files << 'lib/rjbcore.bundle'
  else
    s.platform = Gem::Platform::RUBY
    s.extensions << 'ext/extconf.rb'
  end
end
