require 'rubygems/package_task'

load './rjb.gemspec'

Gem::PackageTask.new(RJB_GEMSPEC) do |pkg|
  pkg.gem_spec = RJB_GEMSPEC
  pkg.need_zip = false
  pkg.need_tar = false
end

desc 'Default Task'
task :default => :package
