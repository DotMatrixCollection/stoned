require "rubygems"
require "rbconfig"

BUNDLER_RB_DIR = File.dirname(File.expand_path(__FILE__))

module Bundler
  VERSION = "2.5.0.stoned"

  class GemfileNotFound < RuntimeError; end
  class GemNotFound     < RuntimeError; end
  class BundlerError    < RuntimeError; end
  class LockfileError   < BundlerError; end
  class PermissionError < BundlerError; end

  class << self
    def root
      pathname_for(root_path)
    end

    def gemfile
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        File.expand_path(gemfile_name)
      else
        File.join(root_path, "Gemfile")
      end
    end

    def lockfile
      File.join(root_path, "Gemfile.lock")
    end

    def default_gemfile
      pathname_for(gemfile)
    end

    def default_lockfile
      pathname_for(lockfile)
    end

    def setup(*groups)
      require File.join(BUNDLER_RB_DIR, "bundler", "setup")
      self
    end

    def require(*groups)
      # Set up load path first
      setup

      gemfile_path = gemfile
      lockfile_path = lockfile
      return unless File.exist?(gemfile_path)
      return unless File.exist?(lockfile_path)

      requested_groups = groups.flatten.map { |group| group.to_s }
      requested_groups = ["default"] if requested_groups.empty?

      parse_gemfile_dependencies(gemfile_path).each do |dep|
        dep_groups = dep[:groups] || ["default"]
        next if (dep_groups & requested_groups).empty?

        req_setting = dep[:require]
        next if req_setting == false
        if req_setting.is_a?(Array)
          req_setting.each { |r| Kernel.send(:require, r) rescue nil }
        elsif req_setting.is_a?(String)
          Kernel.send(:require, req_setting) rescue nil
        else
          Kernel.send(:require, dep[:name]) rescue nil
        end
      end

      self
    end

    def environment
      self
    end

    def settings
      @settings ||= {}
    end

    def bundle_path
      pathname_for(Gem.home)
    end

    def app_config_path
      pathname_for(File.join(root_path, ".bundle"))
    end

    def app_cache
      pathname_for(File.join(root_path, "vendor", "cache"))
    end

    def user_home
      pathname_for(ENV["HOME"] || Dir.pwd)
    end

    def with_unbundled_env(&block)
      block.call if block_given?
    end
    alias with_clean_env with_unbundled_env
    alias clean_env with_unbundled_env

    def original_env
      filtered_env
    end

    def unbundled_env
      filtered_env
    end

    def clean_system(*args)
      system(*args)
    end

    def unbundled_system(*args)
      clean_system(*args)
    end

    def ui
      @ui ||= BundlerUI.new
    end

    def load
      self
    end

    def locked_gems
      nil
    end

    private

    def root_path
      gemfile_name = ENV["BUNDLE_GEMFILE"]
      if gemfile_name && !gemfile_name.empty?
        return File.dirname(File.expand_path(gemfile_name))
      end

      dir = Dir.pwd
      loop do
        return dir if File.exist?(File.join(dir, "Gemfile"))
        parent = File.dirname(dir)
        break if parent == dir
        dir = parent
      end
      raise GemfileNotFound, "Could not locate Gemfile"
    end

    def pathname_for(path)
      Pathname.new(path)
    rescue Exception
      path
    end

    def filtered_env
      filtered = {}
      ENV.to_h.each do |key, value|
        filtered[key] = value unless key.start_with?("BUNDLE_")
      end
      filtered
    end

    def parse_gemfile_dependencies(gemfile_path)
      deps = []
      group_stack = []

      begin
        File.readlines(gemfile_path).each do |line|
          stripped = line.strip
          next if stripped.empty? || stripped.start_with?("#")

          if stripped =~ /^group\s+(.+?)\s+do\s*$/
            groups = $1.scan(/:([A-Za-z0-9_]+)/).flatten
            group_stack << groups
            next
          end

          if stripped == "end"
            group_stack.pop if group_stack.any?
            next
          end

          next unless stripped =~ /^gem\s+['"]([^'"]+)['"](.*)$/

          gem_name = $1
          rest = $2
          dep_groups = []
          group_stack.each { |groups| dep_groups.concat(groups) }
          dep_groups.concat(extract_inline_groups(rest))
          dep_groups = ["default"] if dep_groups.empty?

          deps << {
            name: gem_name,
            groups: dep_groups.uniq,
            require: extract_require_setting(gem_name, rest),
          }
        end
      rescue Exception
        return []
      end

      deps
    end

    def extract_inline_groups(rest)
      groups = []

      if rest =~ /groups?:\s*\[([^\]]*)\]/
        groups.concat($1.scan(/:([A-Za-z0-9_]+)/).flatten)
      end

      if rest =~ /groups?:\s*:([A-Za-z0-9_]+)/
        groups << $1
      end

      groups
    end

    def extract_require_setting(gem_name, rest)
      return gem_name unless rest =~ /require:\s*(false|true|\[[^\]]*\]|"[^"]*"|'[^']*')/

      token = $1
      return false if token == "false"
      return gem_name if token == "true"
      return token.scan(/['"]([^'"]+)['"]/).flatten if token.start_with?("[")

      token[1...-1]
    end
  end

  class BundlerUI
    def info(msg, newline = true); end
    def warn(msg, newline = true); end
    def error(msg, newline = true); end
    def debug(msg, newline = true); end
    def confirm(msg, newline = true); end
    def silence; yield; end
  end
end
