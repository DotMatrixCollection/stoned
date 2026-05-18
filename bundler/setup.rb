require "rubygems"

# bundler/setup — activate bundled gems by reading Gemfile.lock

_bundler_gemfile  = ENV["BUNDLE_GEMFILE"] || "Gemfile"
_bundler_gemfile  = File.expand_path(_bundler_gemfile)
_bundler_root     = File.dirname(_bundler_gemfile)
_bundler_lockfile = File.join(_bundler_root, "Gemfile.lock")

unless File.exist?(_bundler_lockfile)
  raise LoadError, "Could not locate Gemfile.lock. Run `bundle install` first."
end

_current_section  = nil
_path_remote      = nil
_path_gems        = {}
_gem_gems         = {}

File.readlines(_bundler_lockfile).each do |line|
  s = line.chomp
  if s =~ /^(GEM|PATH|GIT|PLATFORMS|DEPENDENCIES|BUNDLED WITH)\s*$/
    _current_section = $1
    _path_remote     = nil
    next
  end
  if _current_section == "PATH" && s =~ /^\s+remote:\s+(\S+)/
    _path_remote = $1
    unless _path_remote.start_with?("/")
      _path_remote = File.expand_path(_path_remote, _bundler_root)
    end
    next
  end
  if s =~ /^\s{4}(\S+)\s+\(([^)]+)\)/
    name = $1
    ver  = $2
    if _current_section == "PATH" && _path_remote
      _path_gems[name] = _path_remote
    elsif _current_section == "GEM"
      _gem_gems[name] = ver
    end
  end
end

# Activate PATH gems from their source directory
_path_gems.each do |name, remote|
  spec_files = Dir.glob(File.join(remote, "*.gemspec"))
  if spec_files.any?
    Gem::Specification.reset!
    Gem::Specification._loading_spec = nil
    begin
      load spec_files.first
      spec = Gem::Specification._loading_spec
      if spec
        (spec.require_paths || ["lib"]).each do |rp|
          lib = File.join(remote, rp)
          $LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
        end
      end
    rescue Exception
      lib = File.join(remote, "lib")
      $LOAD_PATH.unshift(lib) if File.directory?(lib) && !$LOAD_PATH.include?(lib)
    end
  else
    lib = File.join(remote, "lib")
    $LOAD_PATH.unshift(lib) if File.directory?(lib) && !$LOAD_PATH.include?(lib)
  end
end

# Activate installed gems
Gem::Specification.reset!
_gem_gems.each do |name, version|
  spec = Gem::Specification.find_by_name(name, version)
  next unless spec
  (spec.require_paths || ["lib"]).each do |rp|
    lib = File.join(spec.gem_dir, rp)
    $LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib) || !File.directory?(lib)
  end
end
